#pragma once

#include <arm_math.h>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <span>
#include <vector>

namespace sp {

class RfftFastF32 {
public:
    explicit RfftFastF32(size_t fft_len) {
        [[maybe_unused]] const arm_status status = arm_rfft_fast_init_f32(&instance, static_cast<uint16_t>(fft_len));
        assert(status == ARM_MATH_SUCCESS);
    }

    void operator()(std::span<float> input, std::span<float> output) {
        assert(input.size() == instance.fftLenRFFT);
        assert(output.size() == instance.fftLenRFFT);
        arm_rfft_fast_f32(&instance, input.data(), output.data(), 0);
    }

private:
    arm_rfft_fast_instance_f32 instance {};
};

} // namespace sp
