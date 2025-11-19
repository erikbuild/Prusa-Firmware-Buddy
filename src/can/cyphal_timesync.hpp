#pragma once

#include <atomic>

#include <cyphal_suber_call.hpp>
#include <cyphal_task.hpp>
#include "cyphal_port_list.hpp"
#include <uavcan/time/Synchronization_1_0.h>

#include <filters/kalman.hpp>
#include <timing.h>

namespace can::cyphal {

namespace detail {
    consteval double power_of_two(double x) { return x * x; }
} // namespace detail

/**
 * @brief Class for time synchronization.
 * This doesn't fully follow the proposal in data_types/public_regulated_data_types/uavcan/time/7168.Synchronization.1.0.dsdl.
 * This is only the client side of things. The server side is a part of can::Task.
 */
class TimeSync {
    uint32_t filter_index = 0; ///< Index of the CAN Rx filter to use

    struct LocalRemoteSet {
        int64_t local; ///< Local time, local get_timestamp_us() [us]
        int64_t remote; ///< Remote time, time of timesync master [us]
    };

    // Receiving messages, second message has remote time of the first one
    int64_t last_rx_local = 0; ///< Last received timesync sampled by local time [us]
    CanardTransferID last_transfer_id = CANARD_TRANSFER_ID_MAX + 1; ///< Transfer-ID of last timesync message
    CanardNodeID master_node_id = CANARD_NODE_ID_UNSET; ///< Node-ID of the time master

    // Obtaining a set of local and remote timestamps, pass this to loop()
    std::atomic<bool> sync_lock = false; ///< True if the time synchronization is locked with master
    LocalRemoteSet last_sync = { 0, 0 }; ///< A valid set of synchronized timestamps
    std::atomic<bool> last_sync_valid = false; ///< True if last_sync is valid

    // Filtering and other calculation
    LocalRemoteSet previous_sync = { 0, 0 }; ///< Previous valid set of synchronized timestamps
    bool previous_sync_valid = false; ///< True if previous_sync is valid in respect with last_sync

    /**
     * @brief Offset filter smooths out random noise in sampling sets of local-remote timestamps.
     * The offset is valid at time of last_sync.local. After that, drift has to be applied.
     * Offset filter is local-remote time [us].
     *
     * Error measure is a variance in the offset measurement.
     * We expect uncertainty of 1 us and variance of 1 (us)^2 from the algorithm.
     *
     * Error weight is input from absolute prediction error into error estimate.
     * We use measurement uncertainty.
     *
     * Initial error is variance we expect when we reset both filters.
     * On the first sample we do not know the oscillator drift.
     * We expect our oscillator to be 20 ppm or better. The first sample comes 1 s after the filter is reset.
     * During one second the uncertainty is 20 us. Added to it is uncertainty of any measurement.
     */
    KalmanFilter offset_filter = { detail::power_of_two(OFFSET_UNCERTAINTY), OFFSET_UNCERTAINTY, OFFSET_INITIAL_ERROR, std::nan("") };
    static constexpr double OFFSET_UNCERTAINTY = 1.; ///< [us]
    static constexpr double OFFSET_INITIAL_ERROR = detail::power_of_two(OFFSET_UNCERTAINTY + 20.); ///< [(us)^2]

    /**
     * @brief Drift filter compensates for oscillator differences.
     * Drift filter is local-remote frequencies ratio minus one [unitless].
     * Ideal oscillator would have drift 0.
     *
     * Error measure is a variance in the drift measurement.
     * We expect uncertainty 1 us from offset measurement. There are 2 measurements 1 s apart.
     * Uncertainty of 2 * 1 us on 1 s interval makes 2 ppm and variance of 4 (ppm)^2.
     *
     * Error weight is input from absolute prediction error into error estimate.
     * We use measurement uncertainty.
     *
     * Initial error is the variance we expect to be there when we reset the filter.
     * We expect our oscillator to be 20 ppm or better and add any measurement uncertainty.
     */
    KalmanFilter drift_filter = { detail::power_of_two(DRIFT_UNCERTAINTY), DRIFT_UNCERTAINTY, DRIFT_INITIAL_ERROR, 0 };
    static constexpr double DRIFT_UNCERTAINTY = 2 * OFFSET_UNCERTAINTY / 1'000'000.; ///< [unitless]
    static constexpr double DRIFT_INITIAL_ERROR = detail::power_of_two(DRIFT_UNCERTAINTY + 20. / 1'000'000); ///< [unitless^2]

    // Getting remote time from ISR
    struct RemoteData {
        double offset_us; ///< See TimeSync::offset_us
        double average_drift; ///< See TimeSync::average_drift
        int64_t previous_sync_local; ///< See TimeSync::previous_sync.local
    }; ///< Data needed to calculate remote time
    RemoteData remote_data[2] = {}; ///< Two copies of data for safe access from ISR
    std::atomic<bool> remote_data_valid = false; ///< False to indicate get_remote_data[0] is valid, true for get_remote_data[1]

    // State of the syncronization
    std::atomic<bool> sync_valid = false; ///< True if the time synchronization is valid
    int64_t last_calculation_done = 0; ///< Local timestamp when synchronization was calculated [us]

    /// Mutex to access time functions from multiple threads
    StaticSemaphore_t time_mutex_buffer = {};
    SemaphoreHandle_t time_mutex = xSemaphoreCreateMutexStatic(&time_mutex_buffer);

    /// Subscription for time synchronization messages and Rx callback
    SuberCall<uavcan_time_Synchronization_1_0, uavcan_time_Synchronization_1_0_EXTENT_BYTES_> sync_suber;

public:
    /// Loose lock with a single master after this time [us]
    static constexpr int32_t SYNC_LOCK_TIMEOUT_US = 1'000'000 * uavcan_time_Synchronization_1_0_PUBLISHER_TIMEOUT_PERIOD_MULTIPLIER + 500'000;
    static constexpr int32_t SYNC_LOST_TIMEOUT_US = 1'000'000 * 10 + 500'000; ///< Time synchronization is considered lost after this time [us]
    static constexpr double SYNC_MAX_OFFSET_ERROR = detail::power_of_two(OFFSET_UNCERTAINTY * 3); ///< Maximum offset filter error for a precise sync [us^2]
    static_assert(SYNC_MAX_OFFSET_ERROR < OFFSET_INITIAL_ERROR, "SYNC_MAX_OFFSET_ERROR is too high");
    static constexpr double SYNC_MAX_DRIFT_ERROR = detail::power_of_two(DRIFT_UNCERTAINTY * 3); ///< Maximum drift filter error for a precise sync [unitless^2]
    static_assert(SYNC_MAX_DRIFT_ERROR < DRIFT_INITIAL_ERROR, "SYNC_MAX_DRIFT_ERROR is too high");

    /**
     * @brief Construct a new TimeSync client object.
     * @param filter_index_ Index of the CAN Rx filter to use
     */
    TimeSync(uint32_t filter_index_);

    /**
     * @brief Add itself to task and to portlist.
     * @note Does not add port name and type registers (not needed for uavcan types).
     * @param port_list node's object publishing used ports
     */
    void init(PortList &port_list) {
        sync_suber.add_to_task();
        CanardFilter filter = canardMakeFilterForSubject(uavcan_time_Synchronization_1_0_FIXED_PORT_ID_);
        cyphal_task.set_filter(filter_index, filter, true, false);
        port_list.add(sync_suber);
    }

    /**
     * @brief Manage time synchronization.
     * Run this periodically at least several times a second.
     * @param timestamp local time [us], get_timestamp_us()
     * @return true each time we get a new synchronization and we have a precise sync
     */
    bool loop(int64_t timestamp);

    /**
     * @brief Return if the sync state is valid.
     */
    [[nodiscard]] bool is_valid() {
        return sync_valid;
    }

    /**
     * @brief Return if the sync state is valid and the synchronization is precise.
     */
    [[nodiscard]] bool is_precise() {
        return sync_valid && offset_filter.error() < SYNC_MAX_OFFSET_ERROR && drift_filter.error() < SYNC_MAX_DRIFT_ERROR;
    }

    /**
     * @brief Convert remote time to local time.
     * @note Time of remote_timestamp should be close to now.
     * @param remote_timestamp remote time [us]
     * @param timeout mutex timeout
     * @return local time [us], negative -1 if not available or -2 if mutex timeouted
     */
    [[nodiscard]] int64_t get_local(int64_t remote_timestamp, TickType_t timeout = portMAX_DELAY);

    struct SubtickTimestamp {
        int64_t timestamp; ///< Time [us]
        uint32_t subticks; ///< Subticks [1/TIM_BASE_CLK_MHZ us] in range [0, TIM_BASE_CLK_MHZ - 1]
    };

    /**
     * @brief Convert remote time to local time in the same precision as the main clock timer.
     * @note Time of remote_timestamp should be close to now.
     * @param remote_timestamp remote time [us]
     * @param timeout mutex timeout
     * @return local time and subticks, negative {-1, 0} if not available or {-2, 0} if mutex timeouted
     */
    [[nodiscard]] SubtickTimestamp get_local_subtick(int64_t remote_timestamp, TickType_t timeout = portMAX_DELAY);

    /**
     * @brief Convert local time to remote time.
     * @note Time of local_timestamp should be close to now.
     * @param local_timestamp local time [us]
     * @param timeout mutex timeout
     * @return remote time [us], negative -1 if not available or -2 if mutex timeouted
     */
    [[nodiscard]] int64_t get_remote(int64_t local_timestamp, TickType_t timeout = portMAX_DELAY);

    /**
     * @brief Convert local time to remote time from ISR.
     * @note Time of local_timestamp should be close to now.
     * @note This version is to be only used in ISR. It doesn't use mutex and is not safe from tasks.
     * @param local_timestamp local time [us]
     * @return remote time [us], negative -1 if not available
     */
    [[nodiscard]] int64_t get_remote_isr(int64_t local_timestamp);

    /// @return Subscribed port id.
    [[nodiscard]] static constexpr CanardPortID get_subscriber_id() {
        return uavcan_time_Synchronization_1_0_FIXED_PORT_ID_;
    }
};

} // namespace can::cyphal
