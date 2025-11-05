/// \file

#include "screen_menu_version_info.hpp"
#include "config.h"
#include <version/version.hpp>
#include "img_resources.hpp"
#include "shared_config.h" //BOOTLOADER_VERSION_ADDRESS
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include <utils/string_builder.hpp>

#include <option/bootloader.h>
#include <option/has_mmu2.h>

#if HAS_MMU2()
    #include "Marlin/src/feature/prusa/MMU2/mmu2_mk4.h"
#endif

ScreenMenuVersionInfo::ScreenMenuVersionInfo()
    : ScreenMenuVersionInfo__(_(label)) {
    header.SetIcon(&img::info_16x16);

    Item<MI_INFO_FW>().ChangeInformation(version::project_version_full);

    {
        ArrayStringBuilder<12> sb;
#if BOOTLOADER()
        const version_t *bootloader_version = (const version_t *)BOOTLOADER_VERSION_ADDRESS;
        sb.append_printf("%d.%d.%d", bootloader_version->major, bootloader_version->minor, bootloader_version->patch);
#else
        sb.append_string("noboot");
#endif
        Item<MI_INFO_BOOTLOADER>().ChangeInformation(sb.str());
    }

#if HAS_MMU2()
    if (FSensors_instance().HasMMU()) {
        const auto mmu_version = MMU2::mmu2.GetMMUFWVersion();
        if (mmu_version.major != 0) {
            ArrayStringBuilder<12> sb;
            sb.append_printf("%d.%d.%d", mmu_version.major, mmu_version.minor, mmu_version.build);
            Item<MI_INFO_MMU>().ChangeInformation(sb.str());
        } else {
            Item<MI_INFO_MMU>().ChangeInformation("N/A");
        }
        Item<MI_INFO_MMU>().show();
    } else {
        Item<MI_INFO_MMU>().hide();
    }
#endif

    EnableLongHoldScreenAction();
}
