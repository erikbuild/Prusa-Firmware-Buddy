#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <signal_processing/filters.hpp>

#include <cmath>
#include <vector>

// Test parameters:
//   sample_rate = 1000 Hz, cutoff = 100 Hz
//   FIR length = 15 (odd, for symmetric filters)
//   Gaussian sigma = 2.0
//   LeakyMean leak = 0.95

static constexpr float sample_rate = 1000.0f;
static constexpr float cutoff_hz = 100.0f;
static constexpr float tolerance = 1e-5f;
static constexpr size_t N_fir = 15;

// np.random.seed(42); np.random.randn(16)
static const std::vector<float> random_input = {
    0.4967141530f,
    -0.1382643012f,
    0.6476885381f,
    1.5230298564f,
    -0.2341533747f,
    -0.2341369569f,
    1.5792128155f,
    0.7674347292f,
    -0.4694743859f,
    0.5425600436f,
    -0.4634176928f,
    -0.4657297536f,
    0.2419622716f,
    -1.9132802447f,
    -1.7249178325f,
    -0.5622875292f,
};

static void check_approx(const std::vector<float> &actual, const std::vector<float> &expected) {
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        INFO("index " << i);
        CHECK_THAT(actual[i], Catch::Matchers::WithinAbs(expected[i], tolerance));
    }
}

template <typename FilterT>
static std::vector<float> run_filter(FilterT &filter, const std::vector<float> &input) {
    std::vector<float> output;
    output.reserve(input.size());
    for (float x : input) {
        output.push_back(filter.process(x));
    }
    return output;
}

// ============================================================================
// Coefficient generation tests
//
// These tests use double template instantiation (e.g. hamming_lowpass_coeffs<double, N>)
// so that the C++ code uses std::sin/std::cos instead of the CMSIS-DSP arm_sin_f32/
// arm_cos_f32 approximations. The CMSIS LUT-based trig has up to ~1.9e-3 error,
// which would make float-precision coefficients diverge from the scipy reference
// far beyond our tolerance. Using double isolates the algorithm correctness from
// the platform trig implementation.
//
// Python generation script (common preamble):
//   import math
//   fs, fc = 1000.0, 100.0
//   N = 15
//   sigma = 2.0
// ============================================================================

TEST_CASE("Coefficient generation", "[signal_processing][filters]") {

    SECTION("moving_average") {
        // each = 1.0 / N
        auto coeffs = sp::moving_average_coeffs<double, N_fir>();
        double expected = 1.0 / static_cast<double>(N_fir);
        for (size_t i = 0; i < N_fir; ++i) {
            INFO("index " << i);
            CHECK_THAT(coeffs[i], Catch::Matchers::WithinAbs(expected, 1e-10));
        }
    }

    SECTION("butterworth_lowpass_1st") {
        // omega_c = 2*pi*fc; omega_d = 2*fs*tan(omega_c/(2*fs))
        // c = omega_d / (2*fs)
        // a0 = 1+c; b0=b1=c/a0; a1=(c-1)/a0
        auto coeffs = sp::butterworth_lowpass_biquad_1st<double>(cutoff_hz, sample_rate);
        CHECK_THAT(coeffs.b0, Catch::Matchers::WithinAbs(0.2452372753, 1e-8));
        CHECK_THAT(coeffs.b1, Catch::Matchers::WithinAbs(0.2452372753, 1e-8));
        CHECK_THAT(coeffs.b2, Catch::Matchers::WithinAbs(0.0, 1e-10));
        CHECK_THAT(coeffs.a1, Catch::Matchers::WithinAbs(-0.5095254495, 1e-8));
        CHECK_THAT(coeffs.a2, Catch::Matchers::WithinAbs(0.0, 1e-10));
    }

    SECTION("butterworth_lowpass_2nd") {
        // sqrt2 = sqrt(2); a0 = 1+sqrt2*c+c^2
        // b0=b2=c^2/a0; b1=2*c^2/a0; a1=2*(c^2-1)/a0; a2=(1-sqrt2*c+c^2)/a0
        auto coeffs = sp::butterworth_lowpass_biquad_2nd<double>(cutoff_hz, sample_rate);
        CHECK_THAT(coeffs.b0, Catch::Matchers::WithinAbs(0.0674552739, 1e-8));
        CHECK_THAT(coeffs.b1, Catch::Matchers::WithinAbs(0.1349105478, 1e-8));
        CHECK_THAT(coeffs.b2, Catch::Matchers::WithinAbs(0.0674552739, 1e-8));
        CHECK_THAT(coeffs.a1, Catch::Matchers::WithinAbs(-1.1429805025, 1e-8));
        CHECK_THAT(coeffs.a2, Catch::Matchers::WithinAbs(0.4128015981, 1e-8));
    }

    SECTION("butterworth_lowpass_4th") {
        // Two SOS sections with Q values from 4th-order Butterworth poles
        // q1 = 1/(2*sin(3*pi/8)), q2 = 1/(2*sin(pi/8))
        auto coeffs = sp::butterworth_lowpass_biquads_4th<double>(cutoff_hz, sample_rate);

        CHECK_THAT(coeffs[0].b0, Catch::Matchers::WithinAbs(0.0618851953, 1e-8));
        CHECK_THAT(coeffs[0].b1, Catch::Matchers::WithinAbs(0.1237703906, 1e-8));
        CHECK_THAT(coeffs[0].b2, Catch::Matchers::WithinAbs(0.0618851953, 1e-8));
        CHECK_THAT(coeffs[0].a1, Catch::Matchers::WithinAbs(-1.0485995764, 1e-8));
        CHECK_THAT(coeffs[0].a2, Catch::Matchers::WithinAbs(0.2961403576, 1e-8));

        CHECK_THAT(coeffs[1].b0, Catch::Matchers::WithinAbs(0.0779563405, 1e-8));
        CHECK_THAT(coeffs[1].b1, Catch::Matchers::WithinAbs(0.1559126810, 1e-8));
        CHECK_THAT(coeffs[1].b2, Catch::Matchers::WithinAbs(0.0779563405, 1e-8));
        CHECK_THAT(coeffs[1].a1, Catch::Matchers::WithinAbs(-1.3209134308, 1e-8));
        CHECK_THAT(coeffs[1].a2, Catch::Matchers::WithinAbs(0.6327387929, 1e-8));
    }

    SECTION("hamming_lowpass") {
        // Windowed sinc: h[n] = sin(wc*n)/(pi*n) * (0.54 - 0.46*cos(2*pi*(n+M)/(N-1)))
        // Normalized to sum=1
        // from scipy.signal import firwin
        // firwin(15, 100, fs=1000, window='hamming')
        auto coeffs = sp::hamming_lowpass_coeffs<double, N_fir>(cutoff_hz, sample_rate);
        std::vector<double> expected = {
            -0.0035916613,
            -0.0040643978,
            0.0000000000,
            0.0212506970,
            0.0672915323,
            0.1299202042,
            0.1853817637,
            0.2076237240,
            0.1853817637,
            0.1299202042,
            0.0672915323,
            0.0212506970,
            0.0000000000,
            -0.0040643978,
            -0.0035916613,
        };
        REQUIRE(coeffs.size() == expected.size());
        for (size_t i = 0; i < coeffs.size(); ++i) {
            INFO("index " << i);
            CHECK_THAT(coeffs[i], Catch::Matchers::WithinAbs(expected[i], 1e-8));
        }
    }

    SECTION("gaussian_lowpass") {
        // g[n] = exp(-n^2/(2*sigma^2)), normalized to sum=1
        // sigma = 2.0
        auto coeffs = sp::gaussian_lowpass_coeffs<double, N_fir>(2.0);
        std::vector<double> expected = {
            0.0004364074,
            0.0022162598,
            0.0087654775,
            0.0269995714,
            0.0647686048,
            0.1210036840,
            0.1760593214,
            0.1995013476,
            0.1760593214,
            0.1210036840,
            0.0647686048,
            0.0269995714,
            0.0087654775,
            0.0022162598,
            0.0004364074,
        };
        REQUIRE(coeffs.size() == expected.size());
        for (size_t i = 0; i < coeffs.size(); ++i) {
            INFO("index " << i);
            CHECK_THAT(coeffs[i], Catch::Matchers::WithinAbs(expected[i], 1e-8));
        }
    }

    SECTION("gaussian_lowpass_fc") {
        // sigma = 0.133 * fs / fc = 1.33
        // g[n] = exp(-n^2/(2*sigma^2)), normalized to sum=1
        auto coeffs = sp::gaussian_lowpass_coeffs_fc<double, N_fir>(cutoff_hz, sample_rate);
        std::vector<double> expected = {
            0.0000002897,
            0.0000114224,
            0.0002559174,
            0.0032578034,
            0.0235631474,
            0.0968333682,
            0.2260997494,
            0.2999566043,
            0.2260997494,
            0.0968333682,
            0.0235631474,
            0.0032578034,
            0.0002559174,
            0.0000114224,
            0.0000002897,
        };
        REQUIRE(coeffs.size() == expected.size());
        for (size_t i = 0; i < coeffs.size(); ++i) {
            INFO("index " << i);
            CHECK_THAT(coeffs[i], Catch::Matchers::WithinAbs(expected[i], 1e-8));
        }
    }
}

// ============================================================================
// Filter processing tests — impulse response
//
// Reference: scipy.signal.lfilter(b, a, impulse) / sosfilt(sos, impulse)
// Impulse = [1, 0, 0, ...] with 32 samples for IIR, N samples for FIR.
// Coefficients are computed by the C++ code (float precision).
// ============================================================================

TEST_CASE("Filter processing - impulse", "[signal_processing][filters]") {

    SECTION("Biquad - butterworth 1st order") {
        auto coeffs = sp::butterworth_lowpass_biquad_1st<float>(cutoff_hz, sample_rate);
        sp::Biquad<float> filter(coeffs);

        std::vector<float> impulse(32, 0.0f);
        impulse[0] = 1.0f;

        // scipy.signal.lfilter([b0, b1, 0], [1, a1, 0], impulse)
        std::vector<float> expected = {
            0.2452372753f,
            0.3701919082f,
            0.1886221984f,
            0.0961078104f,
            0.0489693753f,
            0.0249511430f,
            0.0127132423f,
            0.0064777205f,
            0.0033005635f,
            0.0016817211f,
            0.0008568797f,
            0.0004366020f,
            0.0002224598f,
            0.0001133489f,
            0.0000577542f,
            0.0000294272f,
            0.0000149939f,
            0.0000076398f,
            0.0000038927f,
            0.0000019834f,
            0.0000010106f,
            0.0000005149f,
            0.0000002624f,
            0.0000001337f,
            0.0000000681f,
            0.0000000347f,
            0.0000000177f,
            0.0000000090f,
            0.0000000046f,
            0.0000000023f,
            0.0000000012f,
            0.0000000006f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("Biquad - butterworth 2nd order") {
        auto coeffs = sp::butterworth_lowpass_biquad_2nd<float>(cutoff_hz, sample_rate);
        sp::Biquad<float> filter(coeffs);

        std::vector<float> impulse(32, 0.0f);
        impulse[0] = 1.0f;

        // scipy.signal.lfilter([b0,b1,b2], [1,a1,a2], impulse)
        std::vector<float> expected = {
            0.0674552739f,
            0.2120106106f,
            0.2819336233f,
            0.2347263156f,
            0.1519049519f,
            0.0767290000f,
            0.0249931441f,
            -0.0031071774f,
            -0.0138686530f,
            -0.0145689522f,
            -0.0109270262f,
            -0.0064752911f,
            -0.0028904376f,
            -0.0006307033f,
            0.0004722957f,
            0.0008001801f,
            0.0007196258f,
            0.0004922027f,
            0.0002655154f,
            0.0001002968f,
            0.0000050322f,
            -0.0000356510f,
            -0.0000428257f,
            -0.0000342322f,
            -0.0000214482f,
            -0.0000103837f,
            -0.0000030146f,
            0.0000008408f,
            0.0000022055f,
            0.0000021737f,
            0.0000015741f,
            0.0000009018f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("BiquadCascade - butterworth 4th order") {
        auto coeffs = sp::butterworth_lowpass_biquads_4th<float>(cutoff_hz, sample_rate);
        sp::BiquadCascade<float, 2> filter(coeffs);

        std::vector<float> impulse(32, 0.0f);
        impulse[0] = 1.0f;

        // scipy.signal.sosfilt(sos, impulse)
        std::vector<float> expected = {
            0.0048243434f,
            0.0307287178f,
            0.0905946820f,
            0.1679448218f,
            0.2246412713f,
            0.2334571879f,
            0.1935125522f,
            0.1237652436f,
            0.0496036031f,
            -0.0085090519f,
            -0.0406738350f,
            -0.0475631979f,
            -0.0368517338f,
            -0.0185628385f,
            -0.0012522191f,
            0.0100331629f,
            0.0139990060f,
            0.0121118272f,
            0.0071218645f,
            0.0017329810f,
            -0.0022227924f,
            -0.0040353573f,
            -0.0039250921f,
            -0.0026318140f,
            -0.0009929459f,
            0.0003536731f,
            0.0010954971f,
            0.0012233213f,
            0.0009227727f,
            0.0004448824f,
            0.0000037902f,
            -0.0002764806f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("FIR - hamming lowpass") {
        auto coeffs = sp::hamming_lowpass_coeffs<float, N_fir>(cutoff_hz, sample_rate);
        sp::FIR<float, N_fir> filter(coeffs);

        std::vector<float> impulse(N_fir, 0.0f);
        impulse[0] = 1.0f;

        // Impulse response of FIR = the coefficients themselves
        // scipy.signal.lfilter(coeffs, [1], impulse)
        std::vector<float> expected = {
            -0.0035916613f,
            -0.0040643978f,
            0.0000000000f,
            0.0212506970f,
            0.0672915323f,
            0.1299202042f,
            0.1853817637f,
            0.2076237240f,
            0.1853817637f,
            0.1299202042f,
            0.0672915323f,
            0.0212506970f,
            0.0000000000f,
            -0.0040643978f,
            -0.0035916613f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("FIR - gaussian lowpass") {
        auto coeffs = sp::gaussian_lowpass_coeffs<float, N_fir>(2.0f);
        sp::FIR<float, N_fir> filter(coeffs);

        std::vector<float> impulse(N_fir, 0.0f);
        impulse[0] = 1.0f;

        // scipy.signal.lfilter(gauss_coeffs, [1], impulse)
        std::vector<float> expected = {
            0.0004364074f,
            0.0022162598f,
            0.0087654775f,
            0.0269995714f,
            0.0647686048f,
            0.1210036840f,
            0.1760593214f,
            0.1995013476f,
            0.1760593214f,
            0.1210036840f,
            0.0647686048f,
            0.0269995714f,
            0.0087654775f,
            0.0022162598f,
            0.0004364074f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("FIR - gaussian lowpass fc") {
        auto coeffs = sp::gaussian_lowpass_coeffs_fc<float, N_fir>(cutoff_hz, sample_rate);
        sp::FIR<float, N_fir> filter(coeffs);

        std::vector<float> impulse(N_fir, 0.0f);
        impulse[0] = 1.0f;

        // sigma = 0.133 * 1000 / 100 = 1.33
        // scipy.signal.lfilter(gauss_fc_coeffs, [1], impulse)
        std::vector<float> expected = {
            0.0000002897f,
            0.0000114224f,
            0.0002559174f,
            0.0032578034f,
            0.0235631474f,
            0.0968333682f,
            0.2260997494f,
            0.2999566043f,
            0.2260997494f,
            0.0968333682f,
            0.0235631474f,
            0.0032578034f,
            0.0002559174f,
            0.0000114224f,
            0.0000002897f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("FIR - moving average") {
        auto coeffs = sp::moving_average_coeffs<float, N_fir>();
        sp::FIR<float, N_fir> filter(coeffs);

        std::vector<float> impulse(N_fir, 0.0f);
        impulse[0] = 1.0f;

        // scipy.signal.lfilter(np.ones(15)/15, [1], impulse)
        std::vector<float> expected = {
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
            0.0666666667f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }

    SECTION("LeakyMean") {
        sp::LeakyMean<float> filter(0.95f);

        std::vector<float> impulse(32, 0.0f);
        impulse[0] = 1.0f;

        // First sample seeds mean=1, output=0. Then mean decays toward 0.
        // mean += 0.95*(x - mean); output = x - mean
        std::vector<float> expected = {
            0.0000000000f,
            -0.0500000000f,
            -0.0025000000f,
            -0.0001250000f,
            -0.0000062500f,
            -0.0000003125f,
            -0.0000000156f,
            -0.0000000008f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
            0.0000000000f,
        };

        check_approx(run_filter(filter, impulse), expected);
    }
}

// ============================================================================
// Filter processing tests — random input
//
// Reference: scipy.signal.lfilter / sosfilt with same coefficients and
// random_input generated by np.random.seed(42); np.random.randn(16).
// ============================================================================

TEST_CASE("Filter processing - random", "[signal_processing][filters]") {

    SECTION("Biquad - butterworth 1st order") {
        auto coeffs = sp::butterworth_lowpass_biquad_1st<float>(cutoff_hz, sample_rate);
        sp::Biquad<float> filter(coeffs);

        // scipy.signal.lfilter([b0,b1,0], [1,a1,0], random_input)
        std::vector<float> expected = {
            0.1218128255f,
            0.1499719996f,
            0.2013443623f,
            0.6349311411f,
            0.6395941316f,
            0.2110472424f,
            0.4373966796f,
            0.7983501896f,
            0.4798507219f,
            0.2624194823f,
            0.1531180591f,
            -0.1498440401f,
            -0.1312254795f,
            -0.4767321872f,
            -1.1351289653f,
            -1.1392851071f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("Biquad - butterworth 2nd order") {
        auto coeffs = sp::butterworth_lowpass_biquad_2nd<float>(cutoff_hz, sample_rate);
        sp::Biquad<float> filter(coeffs);

        // scipy.signal.lfilter([b0,b1,b2], [1,a1,a2], random_input)
        std::vector<float> expected = {
            0.0335059892f,
            0.0959820146f,
            0.1544169297f,
            0.3176637662f,
            0.5327078557f,
            0.5330954457f,
            0.4485588542f,
            0.5416575859f,
            0.6123309973f,
            0.5013142836f,
            0.3304898195f,
            0.1134627560f,
            -0.0845112000f,
            -0.2712657693f,
            -0.6333199863f,
            -1.0115931650f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("BiquadCascade - butterworth 4th order") {
        auto coeffs = sp::butterworth_lowpass_biquads_4th<float>(cutoff_hz, sample_rate);
        sp::BiquadCascade<float, 2> filter(coeffs);

        // scipy.signal.sosfilt(sos, random_input)
        std::vector<float> expected = {
            0.0023963196f,
            0.0145963546f,
            0.0438756479f,
            0.0981448168f,
            0.1927099808f,
            0.3233311725f,
            0.4443350689f,
            0.5197561219f,
            0.5608876871f,
            0.5794774083f,
            0.5571741714f,
            0.4726427764f,
            0.3236654646f,
            0.1279223865f,
            -0.1012294763f,
            -0.3720388565f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("FIR - hamming lowpass") {
        auto coeffs = sp::hamming_lowpass_coeffs<float, N_fir>(cutoff_hz, sample_rate);
        sp::FIR<float, N_fir> filter(coeffs);

        // scipy.signal.lfilter(hamming_coeffs, [1], random_input)
        std::vector<float> expected = {
            -0.0017840290f,
            -0.0015222454f,
            -0.0017643168f,
            0.0024528506f,
            0.0251372442f,
            0.0707856535f,
            0.1453474478f,
            0.2499819650f,
            0.3591517101f,
            0.4430611142f,
            0.4999566501f,
            0.5261138854f,
            0.5199224652f,
            0.4821124382f,
            0.3934106369f,
            0.2503047256f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("FIR - gaussian lowpass") {
        auto coeffs = sp::gaussian_lowpass_coeffs<float, N_fir>(2.0f);
        sp::FIR<float, N_fir> filter(coeffs);

        // scipy.signal.lfilter(gauss_coeffs, [1], random_input)
        std::vector<float> expected = {
            0.0002167697f,
            0.0010405080f,
            0.0043301632f,
            0.0142992242f,
            0.0373889486f,
            0.0813653295f,
            0.1519095089f,
            0.2472301390f,
            0.3520421923f,
            0.4381830185f,
            0.4836516549f,
            0.4946824118f,
            0.4890780359f,
            0.4580740201f,
            0.3718857933f,
            0.2207589080f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("FIR - gaussian lowpass fc") {
        auto coeffs = sp::gaussian_lowpass_coeffs_fc<float, N_fir>(cutoff_hz, sample_rate);
        sp::FIR<float, N_fir> filter(coeffs);

        // scipy.signal.lfilter(gauss_fc_coeffs, [1], random_input)
        std::vector<float> expected = {
            0.0000001439f,
            0.0000056336f,
            0.0001257261f,
            0.0015906521f,
            0.0114367945f,
            0.0473376316f,
            0.1190795190f,
            0.2155319219f,
            0.3588885975f,
            0.5326171998f,
            0.5655836338f,
            0.4518682687f,
            0.4611997503f,
            0.5752645885f,
            0.4981857362f,
            0.2468357452f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("FIR - moving average") {
        auto coeffs = sp::moving_average_coeffs<float, N_fir>();
        sp::FIR<float, N_fir> filter(coeffs);

        // scipy.signal.lfilter(np.ones(15)/15, [1], random_input)
        std::vector<float> expected = {
            0.0331142769f,
            0.0238966568f,
            0.0670758927f,
            0.1686112164f,
            0.1530009914f,
            0.1373918610f,
            0.2426727153f,
            0.2938350306f,
            0.2625367382f,
            0.2987074078f,
            0.2678128949f,
            0.2367642447f,
            0.2528950628f,
            0.1253430465f,
            0.0103485243f,
            -0.0602515878f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }

    SECTION("LeakyMean") {
        sp::LeakyMean<float> filter(0.95f);

        // mean = x[0] for first sample; then mean += 0.95*(x[n]-mean); out = x[n]-mean
        std::vector<float> expected = {
            0.0000000000f,
            -0.0317489227f,
            0.0377101958f,
            0.0456525757f,
            -0.0855765328f,
            -0.0042780057f,
            0.0904535883f,
            -0.0360662249f,
            -0.0636487670f,
            0.0474192831f,
            -0.0479279227f,
            -0.0025119992f,
            0.0352590013f,
            -0.1059991757f,
            0.0041181618f,
            0.0583374233f,
        };

        check_approx(run_filter(filter, random_input), expected);
    }
}
