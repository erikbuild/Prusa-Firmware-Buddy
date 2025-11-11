#pragma once

#include <array>
#include <cstdint>
#include <algorithm>

#include <utils/leaky_bucket.hpp>

class MotorStallFilter {
public:
    constexpr MotorStallFilter() = default;
    ~MotorStallFilter() = default;

    bool ProcessSample(int32_t value);

    void SetDetectionThreshold(float dt) {
        detectionThreshold = dt;
    }

    constexpr float DetectionThreshold() const { return detectionThreshold; }

#ifndef UNITTEST
private:
#endif
    std::array<int32_t, 5> buffer {}; // Buffer to store previous input values
    float detectionThreshold = 700'000.F;
#ifdef UNITTEST
    float filteredValue; // for unit tests, filteredValue is a member of the class allowing better debugging experience
#endif
};

class EMotorStallDetector {
    friend class BlockEStallDetection;
    friend class EStallDetectionStateLatch;

public:
    /// Singleton instance of the E-motor stall detector
    /// Technically, it is not required to have exactly one instance of this class in the firmware,
    /// but it makes it simpler to use.
    static EMotorStallDetector &Instance() {
        static EMotorStallDetector emsd;
        return emsd;
    }

    /// Record and process incoming raw readings from the LoadCell
    void ProcessSample(int32_t value, uint32_t time_us);

    /// @returns raw detected flag disregarding blocked and enabled flags
    constexpr bool DetectedRaw() const { return detected; }

    /// @returns true if a stall has been detected somewhere in the past.
    /// The idea is to keep the filter running almost asynchronnously in the back and ask for the @ref detected flag occasionally.
    /// When the flag has been read by the caller code, it may clear it in order to get a new detected == true in the future.
    constexpr bool DetectedUnreported() const {
        return detected_many && (!reported) && (blocked <= 0) && enabled;
    }

    inline void ClearDetected() {
        detected = false;
        detected_many = false;
        reported = false;
        bucket.reset();
    }

    inline bool Reported() const {
        return reported;
    }

    inline void ClearReported() {
        reported = false;
    }

    constexpr bool Enabled() const {
        return enabled;
    }

    void SetEnabled(bool set = true);

    /// Performs evaluation of various runtime conditions.
    /// When a stall is detected multiple times, sets \p reported to true (even if not enabled)
    /// @returns true when the preceding sequence of load cell data events is considered as a stalled E-motor.
    bool Evaluate(bool movingE, bool directionE);

    void SetDetectionThreshold(float dt) {
        emf.SetDetectionThreshold(dt);
    }

    /// Set ignore parameters.
    ///
    /// @param count Ignore first count events in short batch (report the count + 1).
    /// @param forget_after_ms Forget each unreported one after this amount of ms.
    ///
    /// As an expample, if we set count = 2, forget_after_ms = 500
    /// - If first there are two detections that happen at the same time (for
    ///   simplicity), nothing is reported yet.
    /// - If 500ms elapses, one of them is forgotten (it would take another
    ///   500ms to forget the other one).
    /// - Another detection happens - nothing reported yet (as we have
    ///   forgotten one of the previous ones).
    /// - Yet another detection happens after 100ms (not enough time to forget
    ///   anything yet) -> detection is reported (accumulated 3 events).
    void SetIgnore(uint32_t count, uint32_t forget_after_ms) {
        // The bucket operates in us, because of the sample timer resolution.
        bucket.set_parameters(count, forget_after_ms * 1000);
    }
    constexpr float DetectionThreshold() const { return emf.DetectionThreshold(); }

#ifndef UNITTEST
private:
#endif
    constexpr EMotorStallDetector()
        // Report when there are at least 2 skips in 2 seconds
        // (1 of them "fit" without any report, it takes 2000ms to "forget" each one).
        : bucket(2, 2000000) {}
    ~EMotorStallDetector() = default;

    MotorStallFilter emf;
    /// Used to postpone reporting after X detected spikes in a short time.
    ///
    /// To avoid overzealous reporting, this can be used to fine-tune reporting
    /// to do so only after X detected spikes in a short time.
    LeakyBucket bucket;

    /// Used to prevent the filter from generating further detected events even if they were real - e.g. while doing a toolchange.
    /// Increased/decreased by each blocker. >0 = blocked. <0 should never happen
    int blocked = 0;

    /// true if filter value exceeded the threshold
    bool detected : 1 = false;

    /// true if multiple detections (based on the bucket above) happened.
    bool detected_many : 1 = false;

    /// Set to true when \ref Evaluate returns true.
    bool reported : 1 = false;

    /// Enables/Disables the whole feature - used to control the filter from the UI
    /// Disabled by default, the outer code must enable the filter first (probably after reading a flag from EEPROM)
    bool enabled : 1 = false;
};

class BlockEStallDetection {
public:
    BlockEStallDetection() {
        auto &emsd = EMotorStallDetector::Instance();
        emsd.blocked++;
        emsd.detected = false;
        emsd.detected_many = false;
    }
    ~BlockEStallDetection() {
        auto &emsd = EMotorStallDetector::Instance();
        emsd.blocked--;
        if (emsd.blocked == 0) {
            emsd.bucket.reset();
        }
    }
};

class EStallDetectionStateLatch {
    float detectionThreshold;
    bool enabled;

public:
    EStallDetectionStateLatch() {
        auto &emsd = EMotorStallDetector::Instance();
        enabled = emsd.enabled;
        detectionThreshold = emsd.DetectionThreshold();
    }
    ~EStallDetectionStateLatch() {
        auto &emsd = EMotorStallDetector::Instance();
        emsd.SetEnabled(enabled);
        emsd.ClearDetected(); // disregard everything which happened during the state latch being active
        emsd.SetDetectionThreshold(detectionThreshold);
    }
};
