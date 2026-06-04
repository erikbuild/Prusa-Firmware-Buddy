// Wires CanRemoteSensor to the XBuddyExtension Cyphal bridge.

#include <contactless_offset/can_remote_sensor.hpp>
#include <contactless_offset/tool_sensor.hpp>
#include <puppies/xbuddy_extension.hpp>
#include <puppies/tool_offset_sensor.hpp>

namespace tool_offset {

static void on_stream(uint16_t port_id, std::span<const std::byte> payload, void *ctx) {
    if (CanRemoteSensor::accepts_port(port_id)) {
        static_cast<CanRemoteSensor *>(ctx)->handle_data_frame(payload);
    }
}

static void on_start(void *ctx) {
    auto *sensor = static_cast<CanRemoteSensor *>(ctx);
    buddy::puppies::xbuddy_extension.set_stream_callback(on_stream, sensor);
    buddy::puppies::tool_offset_sensor.set_config(false, true);
}

static void on_stop(void * /*ctx*/) {
    buddy::puppies::tool_offset_sensor.set_config(false, false);
    buddy::puppies::xbuddy_extension.set_stream_callback(nullptr, nullptr);
}

static bool check_hw_error(void *) {
    return buddy::puppies::tool_offset_sensor.has_errors();
}

std::unique_ptr<Sensor> get_default_sensor() {
    auto sensor = std::make_unique<CanRemoteSensor>();
    sensor->set_lifecycle_callbacks(on_start, sensor.get(), on_stop, sensor.get());
    sensor->set_error_check_callback(check_hw_error, nullptr);
    return sensor;
}

} // namespace tool_offset
