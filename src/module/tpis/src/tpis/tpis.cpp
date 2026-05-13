#include <tpis/tpis.hpp>

namespace tpis {

constexpr float degC0asKf = 273.15f;
constexpr fixed degC0asK = fixed(degC0asKf);
constexpr float degC25asKf = 25.f + degC0asKf;
constexpr fixed degC25asK = fixed(degC25asKf);

template <typename T, T min, T max, T step>
consteval auto generate_lookup_table(T (*func)(T)) {
    constexpr size_t size = static_cast<size_t>((max - min) / step) + 1;
    std::array<T, size> table {};
    for (size_t i = 0; i < size; ++i) {
        table[i] = func(min + static_cast<T>(i) * step);
    }
    return table;
}

constexpr float f_exp_f = 4.2f;
constexpr float f(float x) { return std::pow(x, f_exp_f); };
// This function call is used to calculate f for ambient temp (according to datasheet the range is -25C - 80C, but the value is in Kelvin)
// But we might go up to 105C in reality - I don't know if it is possible (if the ADC range won't overflow), but let's be safe
// So let's make the range from -30C to 110C
constexpr float f_mapped(fixed x) {
    static constexpr float min = degC0asKf - 30.f;
    static constexpr fixed min_fixed = degC0asK - 30;
    static constexpr float max = degC0asKf + 110.f;
    static constexpr fixed max_fixed = degC0asK + 110;
    static constexpr size_t step = 2;
    static constexpr auto lookup_table = generate_lookup_table<float, min, max, float(step)>(f);
    if (x < min_fixed || x > max_fixed) {
        return f(float(x));
    } else {
        const size_t index = static_cast<size_t>((x - min_fixed) / step);
        assert(index + 1 < lookup_table.size()); // Should be always true
        const fixed offset = x - (min_fixed + index * step);
        const float ratio = float(offset) / step;
        const float next_diff = lookup_table.at(index + 1) - lookup_table.at(index);
        return lookup_table.at(index) + next_diff * ratio;
    }
}

constexpr float F_exp_f = 1 / f_exp_f;
constexpr float F(float x) { return std::pow(x, F_exp_f); };

SensorData decode_sensor_data(std::span<std::byte, 4> raw_data) {
    uint32_t tp_object = (static_cast<uint32_t>(raw_data[0]) << 8 | static_cast<uint32_t>(raw_data[1])) << 1 | static_cast<uint32_t>(raw_data[2] >> 7);
    uint16_t tp_ambient = (static_cast<uint16_t>(raw_data[2] & std::byte { 0x7f }) << 8) | static_cast<uint16_t>(raw_data[3]);
    return SensorData { .tp_object = tp_object, .tp_ambient = tp_ambient };
}

bool validate_checksum(std::span<const std::byte, 32> data) {
    auto checksum = static_cast<uint16_t>(data[0]);
    const uint16_t expected_checksum = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);
    for (size_t i = 3; i < data.size(); ++i) {
        checksum += static_cast<uint8_t>(data[i]);
    }
    return checksum == expected_checksum;
}

std::optional<CalibrationParameters> decode_calibration_parameters(std::span<std::byte, 32> raw_data) {
    const uint8_t protocol = static_cast<uint8_t>(raw_data[0]);
    if (protocol != 0x3) {
        return std::nullopt;
    }

    if (!validate_checksum(raw_data)) {
        return std::nullopt;
    }

    uint8_t lookup = static_cast<uint8_t>(raw_data[9]);
    if (lookup != 2) {
        return std::nullopt;
    }

    const uint16_t ptat25 = static_cast<uint16_t>(raw_data[10]) << 8 | static_cast<uint16_t>(raw_data[11]);
    const uint16_t raw_m_reg = static_cast<uint16_t>(raw_data[12]) << 8 | static_cast<uint16_t>(raw_data[13]);
    const fixed m = fixed(raw_m_reg) / 100;
    const auto raw_u0_reg = static_cast<uint16_t>(raw_data[14]) << 8 | static_cast<uint16_t>(raw_data[15]);
    const uint32_t u0 = raw_u0_reg + 32768;
    const auto raw_uout1_reg = static_cast<uint16_t>(raw_data[16]) << 8 | static_cast<uint16_t>(raw_data[17]);
    const uint32_t uout1 = raw_uout1_reg * 2;
    const uint8_t t_obj1 = static_cast<uint8_t>(raw_data[18]);

    const auto u_div = static_cast<int32_t>(uout1) - static_cast<int32_t>(u0);
    // NOTE: Expensive float op, but OK since it is ideally only done once at init (on failed comm it tries reinit every 2s)
    const float k_inv = (f(t_obj1 + degC0asKf) - f(degC25asKf)) / static_cast<float>(u_div) * 1.96f; // Emisivity 0.51

    return CalibrationParameters {
        .ptat25 = ptat25,
        .m = m,
        .u0 = u0,
        .uout1 = uout1,
        .t_obj1 = t_obj1,
        .k_inv = k_inv
    };
}

TemperatureReading calculate_temps(SensorData measurement, const CalibrationParameters &calibration) {
    const auto t_ambient_k = degC25asK + fixed(static_cast<int32_t>(measurement.tp_ambient) - calibration.ptat25) / calibration.m;
    const auto val = static_cast<float>(static_cast<int32_t>(measurement.tp_object) - static_cast<int32_t>(calibration.u0)) * calibration.k_inv;
    const float t_obj_k = F(val + f_mapped(t_ambient_k));
    const float object_c = t_obj_k - degC0asKf;
    const float ambient_c = static_cast<float>(t_ambient_k - degC0asK);
    return TemperatureReading {
        .object_temperature_celsius = object_c,
        .ambient_temperature_celsius = ambient_c,
    };
}

} // namespace tpis
