#include "factory_reset.hpp"

#include <common/w25x.hpp>
#include <common/st25dv64k.h>
#include <common/sys.hpp>
#include <crash_dump/dump.hpp>
#include <bootloader/bootloader.hpp>
#include <config_store/store_definition.hpp>
#include <option/bootloader.h>
#include <gui.hpp>
#include <display_helper.h>

#include <option/has_phase_stepping.h>
#if HAS_PHASE_STEPPING()
    #include <feature/phase_stepping/phase_stepping.hpp>
#endif
#include <option/has_e2ee_support.h>
#if HAS_E2EE_SUPPORT()
    #include <e2ee/key.hpp>
#endif

// Check that our presets cover all the item flags
static constexpr auto store_flags = [] {
    journal::ItemFlags store_flags = 0;
    config_store_ns::CurrentStore s {};
    visit_all_struct_fields(s, [&](auto &item) {
        store_flags |= item.flags;
    });

    // Exclude special items, they are special
    store_flags &= ~config_store_ns::ItemFlag::special;

    return store_flags;
}();
static constexpr auto preset_flags = [] {
    journal::ItemFlags preset_flags = 0;
    for (const auto &item : FactoryReset::items_config) {
        preset_flags |= item.item_flags;
    }

    return preset_flags;
}();
static_assert(store_flags == preset_flags);

extern osThreadId displayTaskHandle;

static uint16_t encode_params(bool hard_reset, FactoryReset::ItemBitset items_to_keep) {
    static_assert(std::to_underlying(FactoryReset::Item::_cnt) < 16, "ItemBitset must fit in 15 bits");
    return static_cast<uint16_t>(items_to_keep.to_ulong()) | (hard_reset ? (1 << 15) : 0);
}

static bool decode_hard_reset(uint16_t encoded_params) {
    return encoded_params & (1 << 15);
}

static FactoryReset::ItemBitset decode_items_to_keep(uint16_t encoded_params) {
    return FactoryReset::ItemBitset(encoded_params & ~(1 << 15));
}

[[noreturn]] void FactoryReset::perform(bool hard_reset, ItemBitset items_to_keep) {
    crash_dump::save_message(crash_dump::MsgType::FACTORY_RESET, encode_params(hard_reset, items_to_keep), nullptr, nullptr);
    sys_reset();
}

[[noreturn]] void FactoryReset::perform_internal() {
    const uint16_t encoded_params = crash_dump::load_message_error_code();
    crash_dump::message_set_displayed();

    const bool hard_reset = decode_hard_reset(encoded_params);
    const ItemBitset items_to_keep = decode_items_to_keep(encoded_params);
    assert(osThreadGetId() == displayTaskHandle);

    // Render the screen
    Rect16 progress_rect;
    {
        // Fill background
        render_rect(GuiDefaults::RectScreen, COLOR_BLACK);

        constexpr int l = 16;
        constexpr int r = GuiDefaults::ScreenWidth - 16;

        int y = 16;
        int b = y;

        // Draw the message
        {

            constexpr Font font = Font::normal;
            constexpr int line_h = resource_font_size(font).h;

            b += line_h * (HAS_MINI_DISPLAY() ? 5 : 4);
            const auto text = _("Performing factory reset.\nDo not turn off the printer.");
            render_text_align(Rect16::fromLTRB(l, y, r, b), text, font, COLOR_BLACK, COLOR_WHITE, {}, text_flags { Align_t::Center(), is_multiline::yes });
            y = b;
        }

        {
            b += 12;
            progress_rect = Rect16::fromLTRB(l, y, r, b);
            render_rect(progress_rect, COLOR_GRAY);

            b += 12;
            y = b;
        }

        // Show what items we're keeping and deleting
        if (hard_reset) {
            constexpr Font font = Font::normal;
            constexpr int line_h = resource_font_size(font).h;

            b += line_h;
            render_text_align(Rect16::fromLTRB(l, y, r, b), _("HARD RESET"), font, COLOR_BLACK, COLOR_RED);
            y = b;

        } else {
            constexpr Font font = Font::small;
            constexpr int line_h = resource_font_size(font).h;
            constexpr int sep = resource_font_size(font).w * 8;

            for (int i = 0; i < std::to_underlying(Item::_cnt); i++) {
                b += line_h;
                const bool keep = items_to_keep.test(i);
                render_text_align(Rect16::fromLTRB(l, y, sep, b), _(keep ? N_("Keep") : N_("Reset")), font, COLOR_BLACK, keep ? COLOR_GREEN : COLOR_RED);
                render_text_align(Rect16::fromLTRB(sep + 8, y, r, b), _(items_config[i].title), font, COLOR_BLACK, COLOR_WHITE);
                y = b;
            }
        }
    }

    const auto indicate_progress = [&](int progress_pct) {
        render_rect(Rect16::fromLTWH(progress_rect.Left(), progress_rect.Top(), progress_rect.Width() * progress_pct / 100, progress_rect.Height()), COLOR_ORANGE);
    };

    if (!hard_reset) {
#if HAS_PHASE_STEPPING()
        // Phase stepping is a calibration that is stored on xFlash, not in the config store -> it needs special handling
        // On hard reset, we're clearing the whole xFlash anyway, no point in doing this separately
        if (!items_to_keep.test(std::to_underlying(Item::calibrations))) {

            // Formally speaking, we should access phase_stepping only from the marlin thread.
            // But all this function does is unlink files from the xFlash.
            // These files are only accessed in specific phstep gcodes
            // and it's not worth ensuring that they would happen to overwrite before we enter the final factory reset critical section a few lines below
            phase_stepping::remove_from_persistent_storage();
        }
#endif

#if HAS_E2EE_SUPPORT()
        if (!items_to_keep.test(std::to_underlying(Item::security))) {
            e2ee::remove_all_identities();
            e2ee::remove_key();
        }
#endif

        // Configurations that are out of config store should be wiped here
    }

    indicate_progress(33);

    {
        // freertos::CriticalSection is a bit too restrictive and wouldn't allow us to udpate the display properly
        // To be decently sure that noone pokes with anything, let's instead give this thread the highest priority
        // and reduce other tasks priorities.
        // We cannot suspend them completely, because they might be holding a mutex we will need during the factory reset.
        const auto current_thread = osThreadGetId();
        osThreadSetPriority(current_thread, osPriorityRealtime);

        // Make sure that all other tasks have lower priority
        const auto count = uxTaskGetNumberOfTasks();
        const auto list = new TaskStatus_t[count];

        uxTaskGetSystemState(list, count, nullptr);

        for (size_t i = 0; i < count; i++) {
            const auto handle = list[i].xHandle;
            if (handle != current_thread && osThreadGetPriority(handle) >= osPriorityRealtime) {
                osThreadSetPriority(handle, osPriorityHigh);
            }
        }
    }

    // Wipe EEPROM
    for (uint16_t address = 0; address <= (8096 - 4); address += 4) {
        static constexpr uint32_t empty = ~0;
        st25dv64k_user_write_bytes(address, &empty, 4);
    }

    indicate_progress(66);

    if (hard_reset) {
        // Wipe xFlash
        w25x_chip_erase();

#if BOOTLOADER()
        // Invalidate firmware by erasing part of it
        if (!buddy::bootloader::fw_invalidate()) {
            bsod("Error invalidating firmware");
        }
#endif

    } else if (items_to_keep.any()) {
        // Initialize a blank config store and save there our selection of items.
        // Values of these items are being kept in the RAM.
        config_store().init();

        // Do not do default config_sotre().load_all(), it would overwrite our items in the RAM.
        // Instead we provide a stub loader and migrators, so we only end up initializing the EEPROM structure properly.
        config_store().get_backend().load_all([](const auto &, const auto &) {}, {});

        assert(config_store().get_backend().get_journal_state() == journal::Backend::JournalState::ColdStart);

        journal::ItemFlags exclude_flags = 0;
        for (size_t i = 0; i < std::to_underlying(Item::_cnt); i++) {
            if (!items_to_keep.test(i)) {
                exclude_flags |= items_config[i].item_flags;
            }
        }
        config_store().dump_items(exclude_flags);

        // If we're resetting hw config, make sure that we perform first run config setup
        if (exclude_flags & config_store_ns::ItemFlag::hw_config) {
            config_store().force_default_hw_config.set(true);
        }

        config_store().config_version.set(config_store_ns::CurrentStore::newest_config_version);
    }

    // Indicate that we have finished by a rect of a different color
    indicate_progress(100);

    // Wait a few seconds so that the user can visually verify that the factory reset is complete
    HAL_Delay(3000);

    // Just restart the system
    sys_reset();
}
