#include <catch2/catch_test_macros.hpp>

#include <signal_processing/pipeline.hpp>
#include <signal_processing/generators.hpp>
#include <queue>
#include <vector>

TEST_CASE("Signal Processing Pipeline - Constant Source", "[signal_processing][pipeline]") {
    constexpr float CONSTANT_VALUE = 3.14f;
    sp::pipe::ConstantSource<float> source(CONSTANT_VALUE);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(source.poll() == sp::pipe::PollResult::ready);
        float sample = source.next();
        REQUIRE(sample == CONSTANT_VALUE);
    }
}

TEST_CASE("Signal Processing Pipeline - Container Source", "[signal_processing][pipeline]") {
    std::vector<int> data = { 1, 2, 3, 4, 5 };
    sp::pipe::ContainerSource<std::vector<int>> source(data);

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(source.poll() == sp::pipe::PollResult::ready);
        int sample = source.next();
        REQUIRE(sample == data[i]);
    }

    REQUIRE(source.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("Signal Processing Pipeline - Queue Source", "[signal_processing][pipeline]") {
    sp::pipe::QueueSource<std::queue<int>> source({});
    source.push_sample(10);
    source.push_sample(20);
    source.push_sample(30);

    for (int expected : { 10, 20, 30 }) {
        REQUIRE(source.poll() == sp::pipe::PollResult::ready);
        int sample = source.next();
        REQUIRE(sample == expected);
        source.push_sample(sample + 1);
    }

    for (int expected : { 11, 21, 31 }) {
        REQUIRE(source.poll() == sp::pipe::PollResult::ready);
        int sample = source.next();
        REQUIRE(sample == expected);
    }

    REQUIRE(source.poll() == sp::pipe::PollResult::pending);

    source.finalize();
    REQUIRE(source.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("Signal Processing Pipeline - Transform Node", "[signal_processing][pipeline]") {
    std::vector<int> data = { 1, 2, 3, 4, 5 };

    auto pipeline = sp::pipe::make_source(data)
        | sp::pipe::transform([](int x) { return x * 2; });

    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        int sample = pipeline.next();
        REQUIRE(sample == data[i] * 2);
    }

    REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("Signal Processing Pipeline - Zip Node", "[signal_processing][pipeline]") {
    std::vector<int> data1 = { 1, 2, 3 };
    std::vector<int> data2 = { 10, 20, 30, 40 };

    auto pipeline = sp::pipe::zip(
        sp::pipe::make_source(data1),
        sp::pipe::make_source(data2));

    for (size_t i = 0; i < data1.size(); ++i) {
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [sample1, sample2] = pipeline.next();
        REQUIRE(sample1 == data1[i]);
        REQUIRE(sample2 == data2[i]);
    }

    REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("SignalSource - Heap Storage with Constant Source", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    // Create type-erased source from constant source
    SignalSource<float> erased(ConstantSource<float>(42.0f));

    // Should behave identically to non-erased version
    for (int i = 0; i < 10; ++i) {
        REQUIRE(erased.poll() == sp::pipe::PollResult::ready);
        REQUIRE(erased.next() == 42.0f);
    }
}

TEST_CASE("SignalSource - Heap Storage with Container Source", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    std::vector<int> data = { 10, 20, 30, 40, 50 };

    // Create type-erased source
    SignalSource<int> erased(make_source(data));

    // Should produce same values as non-erased
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(erased.poll() == sp::pipe::PollResult::ready);
        REQUIRE(erased.next() == data[i]);
    }

    REQUIRE(erased.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("SignalSource - Heap Storage with Transform Pipeline", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    std::vector<int> data = { 1, 2, 3, 4, 5 };

    // Create complex pipeline
    auto pipeline = make_source(data)
        | transform([](int x) { return x * 2; })
        | transform([](int x) { return x + 10; });

    // Type erase it
    SignalSource<int> erased(std::move(pipeline));

    // Should produce: (data[i] * 2) + 10
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(erased.poll() == sp::pipe::PollResult::ready);
        REQUIRE(erased.next() == data[i] * 2 + 10);
    }

    REQUIRE(erased.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("SignalSource - Inline Storage with Constant Source", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    // Create type-erased source with inline storage
    SignalSource<float, 128> erased(ConstantSource<float>(3.14f));

    // Should behave identically to heap version
    for (int i = 0; i < 10; ++i) {
        REQUIRE(erased.poll() == sp::pipe::PollResult::ready);
        REQUIRE(erased.next() == 3.14f);
    }
}

TEST_CASE("SignalSource - Heap Storage Move Preserves State", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    std::vector<int> data = { 5, 6, 7 };
    SignalSource<int> source(make_source(data));

    REQUIRE(source.next() == 5);

    SignalSource<int> moved(std::move(source));
    REQUIRE(moved.poll() == sp::pipe::PollResult::ready);
    REQUIRE(moved.next() == 6);

    SignalSource<int> reassigned(make_constant(0));
    reassigned = std::move(moved);
    REQUIRE(reassigned.poll() == sp::pipe::PollResult::ready);
    REQUIRE(reassigned.next() == 7);
    REQUIRE(reassigned.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("SignalSource - Inline Storage Move Preserves State", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    std::vector<int> data = { 8, 9, 10 };
    SignalSource<int, 128> source(make_source(data));

    REQUIRE(source.next() == 8);

    SignalSource<int, 128> moved(std::move(source));
    REQUIRE(moved.poll() == sp::pipe::PollResult::ready);
    REQUIRE(moved.next() == 9);

    SignalSource<int, 128> reassigned(make_constant(0));
    reassigned = std::move(moved);
    REQUIRE(reassigned.poll() == sp::pipe::PollResult::ready);
    REQUIRE(reassigned.next() == 10);
    REQUIRE(reassigned.poll() == sp::pipe::PollResult::done);
}

TEST_CASE("SignalSource - Heap Storage with GaussianNoise", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    // Create noise generator with heap storage
    constexpr float TARGET_RMS = 0.5f;
    SignalSource<float> erased(
        make_gaussian_noise<float>(TARGET_RMS, 42, 0.0f)); // Deterministic seed

    // Generate some samples
    float sum_squares = 0.0f;
    constexpr int NUM_SAMPLES = 10000;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        REQUIRE(erased.poll() == sp::pipe::PollResult::ready); // Noise is infinite
        float sample = erased.next();
        sum_squares += sample * sample;
    }

    // Verify RMS is close to target (within 5%)
    float actual_rms = std::sqrt(sum_squares / NUM_SAMPLES);
    REQUIRE(std::abs(actual_rms - TARGET_RMS) / TARGET_RMS < 0.05f);
}

TEST_CASE("SignalSource - Comparison of Erased vs Non-Erased", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    std::vector<int> data = { 1, 2, 3, 4, 5 };

    // Non-erased pipeline
    auto pipeline_raw = make_source(data)
        | transform([](int x) { return x * 3; });

    // Type-erased version of same pipeline
    auto pipeline_data_copy = data; // Need copy since first pipeline consumes it
    SignalSource<int> pipeline_erased(
        make_source(pipeline_data_copy)
        | transform([](int x) { return x * 3; }));

    // Both should produce identical results
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(pipeline_raw.poll() == pipeline_erased.poll());
        REQUIRE(pipeline_raw.next() == pipeline_erased.next());
    }

    REQUIRE(pipeline_raw.poll() == pipeline_erased.poll());
}

TEST_CASE("SignalSource - Type Erase via Pipe Operator", "[signal_processing][type_erasure]") {
    using namespace sp::pipe;

    std::vector<float> data = { 1.0f, 2.0f, 3.0f };

    // Use type_erase() pipe operator
    auto erased = make_source(data)
        | transform([](float x) { return x * 2.0f; })
        | type_erase<float>();

    // Should work correctly
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(erased.next() == data[i] * 2.0f);
    }
}

TEST_CASE("Signal Processing Pipeline - drop_samples Node", "[signal_processing][pipeline][stream_manipulation]") {
    using namespace sp::pipe;

    SECTION("Drop first 3 samples") {
        std::vector<int> data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        auto pipeline = make_source(data) | drop_samples(3);

        std::vector<int> expected = { 4, 5, 6, 7, 8, 9, 10 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Drop 0 samples (passthrough)") {
        std::vector<int> data = { 1, 2, 3, 4, 5 };
        auto pipeline = make_source(data) | drop_samples(0);

        for (size_t i = 0; i < data.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == data[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Drop more samples than available") {
        std::vector<int> data = { 1, 2, 3 };
        auto pipeline = make_source(data) | drop_samples(10);

        // All samples should be dropped, pipeline should be finished
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Drop all samples") {
        std::vector<int> data = { 1, 2, 3, 4, 5 };
        auto pipeline = make_source(data) | drop_samples(5);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - take_samples Node", "[signal_processing][pipeline][stream_manipulation]") {
    using namespace sp::pipe;

    SECTION("Take first 5 samples") {
        std::vector<int> data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        auto pipeline = make_source(data) | take_samples(5);

        std::vector<int> expected = { 1, 2, 3, 4, 5 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Take 0 samples") {
        std::vector<int> data = { 1, 2, 3, 4, 5 };
        auto pipeline = make_source(data) | take_samples(0);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Take more samples than available") {
        std::vector<int> data = { 1, 2, 3 };
        auto pipeline = make_source(data) | take_samples(10);

        for (size_t i = 0; i < data.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == data[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - drop with duration", "[signal_processing][pipeline][stream_manipulation]") {
    using namespace sp::pipe;
    using namespace std::chrono_literals;

    SECTION("Drop 30ms at 100 Hz (3 samples)") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        auto pipeline = make_source(data, 100.0f) | drop(30ms);

        std::vector<float> expected = { 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Drop 0ms (passthrough)") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f };
        auto pipeline = make_source(data, 100.0f) | drop(0ms);

        for (size_t i = 0; i < data.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == data[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Drop 1s at 1000 Hz (1000 samples)") {
        std::vector<float> data(1005);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<float>(i);
        }

        auto pipeline = make_source(data, 1000.0f) | drop(1s);

        std::vector<float> expected = { 1000.0f, 1001.0f, 1002.0f, 1003.0f, 1004.0f };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - take with duration", "[signal_processing][pipeline][stream_manipulation]") {
    using namespace sp::pipe;
    using namespace std::chrono_literals;

    SECTION("Take 50ms at 100 Hz (5 samples)") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        auto pipeline = make_source(data, 100.0f) | take(50ms);

        std::vector<float> expected = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Take 0ms") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f };
        auto pipeline = make_source(data, 100.0f) | take(0ms);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Take 100ms at 10 Hz (1 sample)") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        auto pipeline = make_source(data, 10.0f) | take(100ms);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        REQUIRE(pipeline.next() == 1.0f);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - Combined stream manipulation", "[signal_processing][pipeline][stream_manipulation]") {
    using namespace sp::pipe;

    SECTION("drop_samples + take_samples") {
        std::vector<int> data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        auto pipeline = make_source(data) | drop_samples(2) | take_samples(4);

        std::vector<int> expected = { 3, 4, 5, 6 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("take_samples + transform") {
        std::vector<int> data = { 1, 2, 3, 4, 5, 6, 7, 8 };
        auto pipeline = make_source(data)
            | take_samples(4)
            | transform([](int x) { return x * 10; });

        std::vector<int> expected = { 10, 20, 30, 40 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("transform + drop_samples + take_samples") {
        std::vector<int> data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        auto pipeline = make_source(data)
            | transform([](int x) { return x * 2; })
            | drop_samples(1)
            | take_samples(5);

        std::vector<int> expected = { 4, 6, 8, 10, 12 }; // (2*2, 3*2, 4*2, 5*2, 6*2)
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - Stream manipulation preserves sampling frequency", "[signal_processing][pipeline][stream_manipulation]") {
    using namespace sp::pipe;

    SECTION("drop_samples preserves sampling frequency") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        constexpr float SAMPLING_FREQ = 44100.0f;
        auto pipeline = make_source(data, SAMPLING_FREQ) | drop_samples(2);

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }

    SECTION("take_samples preserves sampling frequency") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        constexpr float SAMPLING_FREQ = 48000.0f;
        auto pipeline = make_source(data, SAMPLING_FREQ) | take_samples(3);

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }

    SECTION("Combined operations preserve sampling frequency") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f };
        constexpr float SAMPLING_FREQ = 96000.0f;
        auto pipeline = make_source(data, SAMPLING_FREQ)
            | drop_samples(1)
            | take_samples(4)
            | transform([](float x) { return x * 2.0f; });

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }
}

TEST_CASE("Signal Processing Pipeline - chain Node", "[signal_processing][pipeline][chain]") {
    using namespace sp::pipe;

    SECTION("Chain two sources") {
        std::vector<int> data1 = { 1, 2, 3 };
        std::vector<int> data2 = { 4, 5, 6 };

        auto pipeline = chain(
            make_source(data1),
            make_source(data2));

        std::vector<int> expected = { 1, 2, 3, 4, 5, 6 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Chain three sources") {
        std::vector<int> data1 = { 1, 2 };
        std::vector<int> data2 = { 3, 4 };
        std::vector<int> data3 = { 5, 6 };

        auto pipeline = chain(
            make_source(data1),
            make_source(data2),
            make_source(data3));

        std::vector<int> expected = { 1, 2, 3, 4, 5, 6 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Chain with empty source in middle") {
        std::vector<int> data1 = { 1, 2 };
        std::vector<int> data2 = {};
        std::vector<int> data3 = { 3, 4 };

        auto pipeline = chain(
            make_source(data1),
            make_source(data2),
            make_source(data3));

        std::vector<int> expected = { 1, 2, 3, 4 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Chain single source") {
        std::vector<int> data = { 1, 2, 3, 4, 5 };

        auto pipeline = chain(make_source(data));

        for (size_t i = 0; i < data.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == data[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Chain with sampling frequency") {
        std::vector<float> data1 = { 1.0f, 2.0f };
        std::vector<float> data2 = { 3.0f, 4.0f };
        constexpr float SAMPLING_FREQ = 1000.0f;

        auto pipeline = chain(
            make_source(data1, SAMPLING_FREQ),
            make_source(data2, SAMPLING_FREQ));

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);

        std::vector<float> expected = { 1.0f, 2.0f, 3.0f, 4.0f };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - chain with transforms", "[signal_processing][pipeline][chain]") {
    using namespace sp::pipe;

    SECTION("Chain multiple sources with transform") {
        std::vector<int> data1 = { 1, 2, 3 };
        std::vector<int> data2 = { 4, 5, 6 };

        auto pipeline = chain(
                            make_source(data1),
                            make_source(data2))
            | transform([](int x) { return x * 10; });

        std::vector<int> expected = { 10, 20, 30, 40, 50, 60 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Chain with drop and take") {
        std::vector<int> data1 = { 1, 2, 3 };
        std::vector<int> data2 = { 4, 5, 6 };
        std::vector<int> data3 = { 7, 8, 9 };

        auto pipeline = chain(
                            make_source(data1),
                            make_source(data2),
                            make_source(data3))
            | drop_samples(2) // Drop 1, 2
            | take_samples(5); // Take 3, 4, 5, 6, 7

        std::vector<int> expected = { 3, 4, 5, 6, 7 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - chain preserves sampling frequency", "[signal_processing][pipeline][chain]") {
    using namespace sp::pipe;

    SECTION("All sources with same non-zero frequency") {
        std::vector<float> data1 = { 1.0f, 2.0f };
        std::vector<float> data2 = { 3.0f, 4.0f };
        constexpr float SAMPLING_FREQ = 44100.0f;

        auto pipeline = chain(
            make_source(data1, SAMPLING_FREQ),
            make_source(data2, SAMPLING_FREQ));

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }

    SECTION("Mix of zero and non-zero frequencies") {
        std::vector<float> data1 = { 1.0f, 2.0f };
        std::vector<float> data2 = { 3.0f, 4.0f };
        constexpr float SAMPLING_FREQ = 48000.0f;

        auto pipeline = chain(
            make_source(data1, SAMPLING_FREQ),
            make_source(data2, 0.0f)); // Zero frequency should be ignored

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }
}

TEST_CASE("Signal Processing Pipeline - integrate Node", "[signal_processing][pipeline][integrate]") {
    using namespace sp::pipe;

    SECTION("Basic integration (running sum)") {
        std::vector<int> data = { 1, 2, 3, 4, 5 };
        auto pipeline = make_source(data, 1.0f) | integrate();

        std::vector<int> expected = { 1, 3, 6, 10, 15 }; // Running sum
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Integration with initial value") {
        std::vector<int> data = { 1, 2, 3, 4 };
        auto pipeline = make_source(data, 1.0f) | integrate(10);

        std::vector<int> expected = { 11, 13, 16, 20 }; // Starting from 10
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Integration of floats") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f };
        auto pipeline = make_source(data, 1.0f) | integrate();

        std::vector<float> expected = { 1.0f, 3.0f, 6.0f, 10.0f };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Integration with negative values") {
        std::vector<int> data = { 1, -2, 3, -4, 5 };
        auto pipeline = make_source(data, 1.0f) | integrate();

        std::vector<int> expected = { 1, -1, 2, -2, 3 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - differentiate Node", "[signal_processing][pipeline][differentiate]") {
    using namespace sp::pipe;

    SECTION("Basic differentiation") {
        std::vector<int> data = { 1, 3, 6, 10, 15 };
        auto pipeline = make_source(data, 1.0f) | differentiate();

        std::vector<int> expected = { 1, 2, 3, 4, 5 }; // Differences
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Differentiation with initial value") {
        std::vector<int> data = { 5, 7, 10, 14 };
        auto pipeline = make_source(data, 1.0f) | differentiate(3);

        std::vector<int> expected = { 2, 2, 3, 4 }; // Diff from initial 3
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Differentiation of floats") {
        std::vector<float> data = { 1.0f, 3.0f, 6.0f, 10.0f };
        auto pipeline = make_source(data, 1.0f) | differentiate();

        std::vector<float> expected = { 1.0f, 2.0f, 3.0f, 4.0f };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Differentiation of constant signal") {
        std::vector<int> data = { 5, 5, 5, 5, 5 };
        auto pipeline = make_source(data, 1.0f) | differentiate();

        std::vector<int> expected = { 5, 0, 0, 0, 0 }; // First value, then zeros
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - integrate and differentiate inverse", "[signal_processing][pipeline][integrate][differentiate]") {
    using namespace sp::pipe;

    SECTION("Differentiate then integrate") {
        std::vector<int> data = { 1, 3, 6, 10, 15 };

        // Differentiate then integrate should reconstruct (approximately)
        auto pipeline = make_source(data, 1.0f)
            | differentiate()
            | integrate();

        std::vector<int> expected = { 1, 3, 6, 10, 15 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Integrate then differentiate") {
        std::vector<int> data = { 1, 2, 3, 4, 5 };

        // Integrate then differentiate should reconstruct original
        auto pipeline = make_source(data, 1.0f)
            | integrate()
            | differentiate();

        std::vector<int> expected = { 1, 2, 3, 4, 5 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - integrate/differentiate with transforms", "[signal_processing][pipeline][integrate][differentiate]") {
    using namespace sp::pipe;

    SECTION("Transform then integrate") {
        std::vector<int> data = { 1, 2, 3, 4 };
        auto pipeline = make_source(data, 1.0f)
            | transform([](int x) { return x * 2; }) // 2, 4, 6, 8
            | integrate(); // 2, 6, 12, 20

        std::vector<int> expected = { 2, 6, 12, 20 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Differentiate then transform") {
        std::vector<int> data = { 0, 1, 3, 6, 10 };
        auto pipeline = make_source(data, 1.0f)
            | differentiate() // 0, 1, 2, 3, 4
            | transform([](int x) { return x * 10; }); // 0, 10, 20, 30, 40

        std::vector<int> expected = { 0, 10, 20, 30, 40 };
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            REQUIRE(pipeline.next() == expected[i]);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - integrate/differentiate preserve sampling frequency", "[signal_processing][pipeline][integrate][differentiate]") {
    using namespace sp::pipe;

    SECTION("Integrate preserves sampling frequency") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f };
        constexpr float SAMPLING_FREQ = 1000.0f;
        auto pipeline = make_source(data, SAMPLING_FREQ) | integrate();

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }

    SECTION("Differentiate preserves sampling frequency") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f };
        constexpr float SAMPLING_FREQ = 2000.0f;
        auto pipeline = make_source(data, SAMPLING_FREQ) | differentiate();

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }

    SECTION("Combined operations preserve sampling frequency") {
        std::vector<float> data = { 1.0f, 2.0f, 3.0f, 4.0f };
        constexpr float SAMPLING_FREQ = 44100.0f;
        auto pipeline = make_source(data, SAMPLING_FREQ)
            | integrate()
            | differentiate()
            | transform([](float x) { return x * 2.0f; });

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }
}

TEST_CASE("Signal Processing Pipeline - zip_longest_tail Node", "[signal_processing][pipeline][zip_longest_tail]") {
    using namespace sp::pipe;

    SECTION("Two sources of different lengths") {
        std::vector<int> short_vec = { 1, 2, 3 };
        std::vector<int> long_vec = { 10, 20, 30, 40, 50 };

        auto pipeline = zip_longest_tail(
            make_source(short_vec),
            make_source(long_vec));

        // First 3 samples should zip normally
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a1, b1] = pipeline.next();
        REQUIRE(a1 == 1);
        REQUIRE(b1 == 10);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a2, b2] = pipeline.next();
        REQUIRE(a2 == 2);
        REQUIRE(b2 == 20);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a3, b3] = pipeline.next();
        REQUIRE(a3 == 3);
        REQUIRE(b3 == 30);

        // Next 2 samples should repeat last value from short_vec (3)
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a4, b4] = pipeline.next();
        REQUIRE(a4 == 3); // Repeats last value
        REQUIRE(b4 == 40);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a5, b5] = pipeline.next();
        REQUIRE(a5 == 3); // Repeats last value
        REQUIRE(b5 == 50);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Three sources of different lengths") {
        std::vector<float> v1 = { 1.0f, 2.0f };
        std::vector<float> v2 = { 10.0f, 20.0f, 30.0f };
        std::vector<float> v3 = { 100.0f, 200.0f, 300.0f, 400.0f };

        auto pipeline = zip_longest_tail(
            make_source(v1),
            make_source(v2),
            make_source(v3));

        // Sample 0: all sources active
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a1, b1, c1] = pipeline.next();
        REQUIRE(a1 == 1.0f);
        REQUIRE(b1 == 10.0f);
        REQUIRE(c1 == 100.0f);

        // Sample 1: all sources active
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a2, b2, c2] = pipeline.next();
        REQUIRE(a2 == 2.0f);
        REQUIRE(b2 == 20.0f);
        REQUIRE(c2 == 200.0f);

        // Sample 2: v1 repeats, others active
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a3, b3, c3] = pipeline.next();
        REQUIRE(a3 == 2.0f); // Repeats
        REQUIRE(b3 == 30.0f);
        REQUIRE(c3 == 300.0f);

        // Sample 3: v1 and v2 repeat, v3 active
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a4, b4, c4] = pipeline.next();
        REQUIRE(a4 == 2.0f); // Repeats
        REQUIRE(b4 == 30.0f); // Repeats
        REQUIRE(c4 == 400.0f);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("All sources same length") {
        std::vector<int> v1 = { 1, 2, 3 };
        std::vector<int> v2 = { 4, 5, 6 };
        std::vector<int> v3 = { 7, 8, 9 };

        auto pipeline = zip_longest_tail(
            make_source(v1),
            make_source(v2),
            make_source(v3));

        for (int i = 0; i < 3; ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            auto [a, b, c] = pipeline.next();
            REQUIRE(a == 1 + i);
            REQUIRE(b == 4 + i);
            REQUIRE(c == 7 + i);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("One source much longer than others") {
        std::vector<int> v1 = { 1 };
        std::vector<int> v2 = { 10, 20, 30, 40, 50, 60, 70 };

        auto pipeline = zip_longest_tail(
            make_source(v1),
            make_source(v2));

        // First sample: both active
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a1, b1] = pipeline.next();
        REQUIRE(a1 == 1);
        REQUIRE(b1 == 10);

        // Remaining samples: v1 repeats 1
        for (int i = 1; i < 7; ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            auto [a, b] = pipeline.next();
            REQUIRE(a == 1); // Always repeats
            REQUIRE(b == 10 + i * 10);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - zip_longest_tail preserves sampling frequency", "[signal_processing][pipeline][zip_longest_tail]") {
    using namespace sp::pipe;

    SECTION("All sources with same sampling frequency") {
        std::vector<float> v1 = { 1.0f };
        std::vector<float> v2 = { 2.0f, 3.0f };
        constexpr float SAMPLING_FREQ = 200.0f;

        auto pipeline = zip_longest_tail(
            make_source(v1, SAMPLING_FREQ),
            make_source(v2, SAMPLING_FREQ));

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }

    SECTION("Mix of zero and non-zero frequencies") {
        std::vector<float> v1 = { 1.0f, 2.0f };
        std::vector<float> v2 = { 3.0f, 4.0f, 5.0f };
        constexpr float SAMPLING_FREQ = 1000.0f;

        auto pipeline = zip_longest_tail(
            make_source(v1, SAMPLING_FREQ),
            make_source(v2, 0.0f)); // Zero frequency should be ignored

        REQUIRE(pipeline.sampling_freq() == SAMPLING_FREQ);
    }
}

TEST_CASE("Signal Processing Pipeline - zip_longest_tail with transforms", "[signal_processing][pipeline][zip_longest_tail]") {
    using namespace sp::pipe;

    SECTION("Transform after zip_longest_tail") {
        std::vector<int> v1 = { 1, 2 };
        std::vector<int> v2 = { 10, 20, 30 };

        auto pipeline = zip_longest_tail(
                            make_source(v1),
                            make_source(v2))
            | transform([](std::tuple<int, int> t) {
                  auto [a, b] = t;
                  return a + b;
              });

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        REQUIRE(pipeline.next() == 11); // 1 + 10

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        REQUIRE(pipeline.next() == 22); // 2 + 20

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        REQUIRE(pipeline.next() == 32); // 2 (repeated) + 30

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Transform before zip_longest_tail") {
        std::vector<int> v1 = { 1, 2 };
        std::vector<int> v2 = { 5, 10, 15 };

        auto s1 = make_source(v1) | transform([](int x) { return x * 10; });
        auto s2 = make_source(v2) | transform([](int x) { return x * 2; });

        auto pipeline = zip_longest_tail(std::move(s1), std::move(s2));

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a1, b1] = pipeline.next();
        REQUIRE(a1 == 10); // 1 * 10
        REQUIRE(b1 == 10); // 5 * 2

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a2, b2] = pipeline.next();
        REQUIRE(a2 == 20); // 2 * 10
        REQUIRE(b2 == 20); // 10 * 2

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a3, b3] = pipeline.next();
        REQUIRE(a3 == 20); // 2 * 10 (repeated)
        REQUIRE(b3 == 30); // 15 * 2

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}

TEST_CASE("Signal Processing Pipeline - zip_longest_tail edge cases", "[signal_processing][pipeline][zip_longest_tail]") {
    using namespace sp::pipe;

    SECTION("Single source") {
        std::vector<int> v1 = { 1, 2, 3 };

        auto pipeline = zip_longest_tail(make_source(v1));

        for (int i = 0; i < 3; ++i) {
            REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
            auto [a] = pipeline.next();
            REQUIRE(a == i + 1);
        }

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }

    SECTION("Two sources where first is longer") {
        std::vector<int> v1 = { 1, 2, 3, 4, 5 };
        std::vector<int> v2 = { 10, 20 };

        auto pipeline = zip_longest_tail(
            make_source(v1),
            make_source(v2));

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a1, b1] = pipeline.next();
        REQUIRE(a1 == 1);
        REQUIRE(b1 == 10);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a2, b2] = pipeline.next();
        REQUIRE(a2 == 2);
        REQUIRE(b2 == 20);

        // v2 is exhausted, should repeat 20
        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a3, b3] = pipeline.next();
        REQUIRE(a3 == 3);
        REQUIRE(b3 == 20);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a4, b4] = pipeline.next();
        REQUIRE(a4 == 4);
        REQUIRE(b4 == 20);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::ready);
        auto [a5, b5] = pipeline.next();
        REQUIRE(a5 == 5);
        REQUIRE(b5 == 20);

        REQUIRE(pipeline.poll() == sp::pipe::PollResult::done);
    }
}
