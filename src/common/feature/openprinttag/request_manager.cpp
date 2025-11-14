/// @file
#include <feature/openprinttag/request_manager.hpp>

#include <bsod/bsod.h>
#include <cstring>
#include <feature/openprinttag/detail/requests_base.hpp>
#include <feature/openprinttag/tool_tag.hpp>
#include <modbus/traits.hpp>
#include <mutex>
#include <prusa3d/nfc/command/AcceptEvent_1_0.h>
#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/nfc/event/Event_1_0.h>
#include <logging/log.hpp>
#include <timing.h>

LOG_COMPONENT_REF(OpenPrintTag);

static constexpr int32_t request_timeout_ms = 5000;

static void serialize_enable_radio(uint16_t request_id, anfc::modbus::Request &request) {
    prusa3d_nfc_command_Request_Request_1_0 object;
    memset(&object, 0, sizeof(object));
    object.request_id.value = request_id;
    prusa3d_nfc_request_RequestData_1_0_select_enable_radio_(&object.request);
    request.data = {};
    auto buffer = std::as_writable_bytes(std::span { request.data });
    size_t size = buffer.size();
    if (prusa3d_nfc_command_Request_Request_1_0_serialize_(&object, reinterpret_cast<uint8_t *>(buffer.data()), &size) == 0) {
        request.size = static_cast<uint16_t>(size);
    } else {
        bsod_unreachable();
    }
}

static void serialize_forget_tag(uint16_t request_id, buddy::openprinttag::TagID tag_id, anfc::modbus::Request &request) {
    prusa3d_nfc_command_Request_Request_1_0 object;
    memset(&object, 0, sizeof(object));
    object.request_id.value = request_id;
    object.request.forget_tag.tag.value = tag_id;
    prusa3d_nfc_request_RequestData_1_0_select_forget_tag_(&object.request);
    request.data = {};
    auto buffer = std::as_writable_bytes(std::span { request.data });
    size_t size = buffer.size();
    if (prusa3d_nfc_command_Request_Request_1_0_serialize_(&object, reinterpret_cast<uint8_t *>(buffer.data()), &size) == 0) {
        request.size = static_cast<uint16_t>(size);
    } else {
        bsod_unreachable();
    }
}

static void serialize_accept_event(uint16_t event_id, anfc::modbus::AcceptEvent &accept_event) {
    prusa3d_nfc_command_AcceptEvent_Request_1_0 object;
    memset(&object, 0, sizeof(object));
    object.event_id.value = event_id;
    accept_event.data = {};
    auto buffer = std::as_writable_bytes(std::span { accept_event.data });
    size_t size = buffer.size();
    if (prusa3d_nfc_command_AcceptEvent_Request_1_0_serialize_(&object, reinterpret_cast<uint8_t *>(buffer.data()), &size) == 0) {
        accept_event.size = static_cast<uint16_t>(size);
    } else {
        bsod_unreachable();
    }
}

namespace buddy::openprinttag {

ToolTag::UIDHash Manager::TagUID::hash() const {
    uint16_t h = (data[0] << 8 | data[1])
        ^ (data[2] << 8 | data[3])
        ^ (data[4] << 8 | data[5])
        ^ (data[6] << 8 | data[7]);
    return h | 1; // make sure to never return no_tag_hash
}

Manager::Manager() {
    for (auto &device : devices) {
        device.manager = this;
    }

    /// For now we support only anfc device 0 for virtual tool 0
    devices[VirtualToolIndex::from_raw(0)].device = anfc::Device::anfc0;
}

bool Manager::step(anfc::modbus::Client &client) {
    std::lock_guard lock { mutex };

    check_timeouts();

    return std::ranges::all_of(devices, [&client](DeviceState &device_state) {
        return device_state.step(client);
    });
}

void Manager::on_request_done(RequestID request_id, std::span<const std::byte> raw_event_data) {
    for (auto &entry : active_requests) {
        if (entry.request && entry.request_id == request_id) {
            entry.request->complete(raw_event_data);
            entry.request = nullptr;
            return;
        }
    }

    log_error(OpenPrintTag, "stray request %d", request_id.to_underlying());
}

void Manager::DeviceState::on_request_done(RequestID request_id, std::span<const std::byte> raw_event_data) {
    // handle enable_radio completion
    if (enable_radio_request_id == request_id) {
        enable_radio_request_id = std::nullopt;
        radio_enabled = true;
        return;
    }

    // handle forget_tag completion
    if (auto *f = std::get_if<TagForgetting>(&tag); f && f->request_id == request_id) {
        tag = TagUnused {};
        return;
    }

    // delegate to manager for regular requests
    manager->on_request_done(request_id, raw_event_data);
}

void Manager::DeviceState::on_tag_detected(TagID tag_id, TagUID tag_uid) {
    if (std::holds_alternative<TagUnused>(tag)) {
        tag = TagDetected { tag_id, tag_uid };
    } else {
        // BFW-8253 will ensure we never get more than one tag per antenna
        // Until then, we log and do nothing.
        log_info(OpenPrintTag, "ignoring tag detected event for tag %d", tag_id);
    }
}

void Manager::DeviceState::on_tag_lost(TagID tag_id) {
    if (auto *d = std::get_if<TagDetected>(&tag); d && d->tag_id == tag_id) {
        tag = TagLost { d->tag_id };
    } else {
        // BFW-8253 will ensure we never get more than one tag per antenna
        // Until then, we log and do nothing.
        log_info(OpenPrintTag, "ignoring tag lost event for tag %d", tag_id);
    }
}

void Manager::DeviceState::forget_lost_tag(anfc::modbus::Client &client) {
    if (!device) {
        return;
    }
    if (auto *lost = std::get_if<TagLost>(&tag)) {
        const auto req_id = manager->make_request_id();
        anfc::modbus::Request request;
        serialize_forget_tag(req_id.to_underlying(), lost->tag_id, request);
        if (client.write(*device, request)) {
            tag = TagForgetting { req_id };
        } else {
            // will be forgotten next time
        }
    }
}

void Manager::handle_pending_request(anfc::modbus::Client &client) {
    if (pending_requests.empty()) {
        return;
    }

    buddy::openprinttag::Request &pending_request = pending_requests.back();
    const ToolTag &tool_tag = pending_request.tool_tag();
    const DeviceState &device = devices[tool_tag.tool()];
    if (auto *d = std::get_if<TagDetected>(&device.tag); d && d->tag_uid.hash() == tool_tag.uid_hash() && device.device) {
        return handle_pending_request(client, pending_request, *device.device, d->tag_id);
    }

    log_warning(OpenPrintTag, "tag not found for request");
    pending_requests.remove(pending_request);
    pending_request.set_finished(std::unexpected(Request::Error::other));
}

void Manager::handle_pending_request(anfc::modbus::Client &client, Request &pending_request, anfc::Device device, TagID tag_id) {
    for (ActiveRequestEntry &entry : active_requests) {
        if (entry.request == nullptr) {
            return handle_pending_request(client, pending_request, device, tag_id, entry);
        }
    }
    // no free slot, try again later
}

void Manager::handle_pending_request(anfc::modbus::Client &client, Request &pending_request, anfc::Device device, TagID tag_id, ActiveRequestEntry &entry) {
    const RequestID request_id = make_request_id();
    anfc::modbus::Request modbus_request = {};
    pending_request.serialize(request_id, tag_id, modbus_request);
    if (client.write(device, modbus_request)) {
        pending_requests.remove(pending_request);
        entry = ActiveRequestEntry {
            .request = &pending_request,
            .request_id = request_id,
            .sent_at = ticks_ms(),
        };
    } else {
        log_warning(OpenPrintTag, "failed to write request");
        // keep pending request at its position in queue and try later
    }
}

bool Manager::DeviceState::step(anfc::modbus::Client &client) {
    if (!device) {
        return true;
    }

    anfc::modbus::Event modbus_event;
    if (!client.read(*device, modbus_event)) {
        return false;
    }

    if (!radio_enabled && !enable_radio_request_id) {
        enable_radio_request_id = manager->make_request_id();
        anfc::modbus::Request request;
        serialize_enable_radio(enable_radio_request_id->to_underlying(), request);
        if (!client.write(*device, request)) {
            enable_radio_request_id = std::nullopt;
            return false;
        }
        // Continue to process events (to receive the ack)
    }

    // Only process other requests after radio is enabled
    if (radio_enabled) {
        manager->handle_pending_request(client);
        forget_lost_tag(client);
    }

    return handle_event(client, modbus_event);
}

bool Manager::DeviceState::handle_event(anfc::modbus::Client &client, const anfc::modbus::Event &modbus_event) {
    if (modbus_event.size == 0) {
        return true;
    }

    // Deserialize event from modbus data
    const auto raw_event_data = ::modbus::payload(modbus_event);

    prusa3d_nfc_event_Event_1_0 event;
    size_t event_size = raw_event_data.size();
    if (prusa3d_nfc_event_Event_1_0_deserialize_(&event, reinterpret_cast<const uint8_t *>(raw_event_data.data()), &event_size) != 0) {
        return true; // Ignore malformed events
    }

    // dispatch event to correct handler
    auto on_event = [&](const prusa3d_nfc_event_EventData_1_0 &event_data) {
        if (prusa3d_nfc_event_EventData_1_0_is_request_done_(&event_data)) {
            const auto &request_done = event_data.request_done;

            // unpack arguments
            const RequestID request_id = RequestID { request_done.request_id.value };

            // handle the event
            return on_request_done(request_id, raw_event_data);
        }

        if (prusa3d_nfc_event_EventData_1_0_is_tag_detected_(&event_data)) {
            const auto &tag_detected = event_data.tag_detected;

            // unpack arguments
            const TagID tag_id = tag_detected.tag.value;
            TagUID tag_uid = {};
            static_assert(sizeof(tag_uid.data) == sizeof(tag_detected.uid));
            std::memcpy(tag_uid.data, tag_detected.uid, sizeof(tag_uid.data));

            // handle the event
            return on_tag_detected(tag_id, tag_uid);
        }

        if (prusa3d_nfc_event_EventData_1_0_is_tag_lost_(&event_data)) {
            const auto &tag_lost = event_data.tag_lost;

            // unpack arguments
            const TagID tag_id = tag_lost.tag.value;

            // handle the event
            return on_tag_lost(tag_id);
        }

        // If this ever happens, it means either serious memory corruption
        // occured or somebody hijacked the bus. Just crash here, it is not
        // safe to proceed anyway.
        bsod_unreachable();
    };

    on_event(event.data);
    anfc::modbus::AcceptEvent accept_event;
    serialize_accept_event(event.event_id.value, accept_event);
    return client.write(*device, accept_event);
}

RequestID Manager::make_request_id() {
    // Note: overflow is OK given current request rate
    return RequestID { ++request_id };
}

void Manager::add_request(Badge<Request>, Request &request) {
    std::lock_guard lock { mutex };

    remove_request_nolock(request);
    pending_requests.push_front(request);
}

void Manager::remove_request(Badge<Request>, Request &request) {
    std::lock_guard lock { mutex };

    remove_request_nolock(request);
}

void Manager::remove_request_nolock(Request &request) {
    pending_requests.remove(request);
    for (auto &entry : active_requests) {
        if (entry.request == &request) {
            entry.request = nullptr;
            break;
        }
    }
}

void Manager::check_timeouts() {
    const auto now = ticks_ms();
    for (auto &entry : active_requests) {
        if (entry.request && ticks_diff(now, entry.sent_at) > request_timeout_ms) {
            log_warning(OpenPrintTag, "request %d timed out", entry.request_id.to_underlying());
            entry.request->set_finished(std::unexpected(Request::Error::other));
            entry.request = nullptr;
        }
    }
}

std::optional<Manager::TagUID> Manager::get_tag_uid_for_tool(VirtualToolIndex tool) {
    std::lock_guard lock { mutex };

    const auto &device = devices[tool];
    if (auto *d = std::get_if<TagDetected>(&device.tag)) {
        return d->tag_uid;
    }
    return std::nullopt;
}

Manager &manager() {
    static Manager instance;
    return instance;
}

} // namespace buddy::openprinttag
