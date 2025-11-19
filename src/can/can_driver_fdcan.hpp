#pragma once

#include <can_driver.hpp>
#include <device/hal.h>

#include <atomic>

namespace can {

/**
 * @brief Implementation of CAN driver for STs FDCAN.
 */
class FdcanDriver : public Driver {
    FDCAN_HandleTypeDef &hfdcan; ///< HAL CAN instance

    CanardMicrosecond tx_timestamp = 0; ///< Timestamp of last sent frame
    std::atomic<uint8_t> tx_timestamp_to_write = 0; ///< Marker of frame to write, 0 means already written

    int64_t rx1_timestamp = 0; ///< Timestamps of last frame received on Rx FIFO 1
    std::atomic<bool> rx1_timestamp_valid = false; ///< True if rx1_timestamp is valid

    uint32_t last_tx_priority = INT32_MAX; ///< Priority of last sent frame

    static FdcanDriver *list; ///< List of all instances for lookup from interrupt
    FdcanDriver *next; ///< Next instance in the list

    uint8_t buffer[CANARD_MTU_CAN_FD]; ///< Buffer for received frames

    bool enable_bit_rate_switch; ///< Enable bit rate switch?

    /**
     * @brief Sanitize message timestamp.
     * @param time_isr time sampled in the ISR as close to the event as possible, needs to be less than TIM3_OVERFLOW apart from frame_timestamp [us]
     * @param frame_timestamp timestamp made by hardware, overflows TIM3_OVERFLOW [us]
     * @return sanitized timestamp in system time
     */
    CanardMicrosecond sanitize_timestamp(int64_t time_isr, uint32_t frame_timestamp);

public:
    /**
     * @param hfdcan_ HAL CAN instance
     * @param enable_bit_rate_switch_ Enable bit rate switch?
     */
    FdcanDriver(FDCAN_HandleTypeDef &hfdcan_, bool enable_bit_rate_switch_ = true)
        : hfdcan(hfdcan_)
        , next(list)
        , enable_bit_rate_switch { enable_bit_rate_switch_ } {
        list = this;
    }

    // Intentionally not virtual
    // Vitrual destructors generate a free() call, not acceptable for no dynamic allocation targets
    ~FdcanDriver() = default;

    /**
     * @brief Do last minute inits and start CAN driver to be active on the bus.
     * @param automatic_retransmission_enable true to enable automatic retransmission after Tx failure, false to disable
     * @note Throw bsod on hardware error.
     */
    void start(bool automatic_retransmission_enable = true) override;

    /**
     * @brief Enable or disable retransmission after failed Tx.
     * @param enable true to retransmit until successful, false to send once and give up
     */
    void set_automatic_retransmission(bool enable) override;

    /**
     * @brief Get CAN frame and put it to hardware Tx queue.
     * @param frame CAN frame
     * @param timestamp true if the frame should be timestamped
     * @return true on success, false if there is no space in the queue
     * @note Throw bsod on hardware error.
     * @note Functions send() and get_sent_timestamp() should be called from the same thread.
     */
    bool send(const CanardFrame &frame, bool store_timestamp = false) override;

    /**
     * @brief Get timestamp of last frame that had store_timestamp set to true.
     * @return timestamp of the last frame or 0 if the Tx event was lost, nullopt if not yet sent
     * @note Functions send() and get_sent_timestamp() should be called from the same thread.
     */
    std::optional<CanardMicrosecond> get_sent_timestamp() override;

    /**
     * @brief Get CAN frame from hardware Rx queue.
     * @param[out] frame CAN frame
     * @warning Buffer for frame.payload is overwritten when this function is called second time.
     * @param[out] timestamp_us timestamp of the received frame
     * @return true on success, false if there is no frame in the queue
     * @note Throw bsod on hardware error.
     */
    bool receive(CanardFrame &frame, CanardMicrosecond *timestamp_us = nullptr) override;

    /**
     * @brief Get number of available filters.
     * @return number of indexes that can be used in set_filter()
     */
    uint32_t filter_count() const override {
        return hfdcan.Init.ExtFiltersNbr;
    }

    /**
     * @brief Set filter for incoming frames.
     * @param index filter index, has to be less than filter_count()
     * @param filter mask and id of the filter
     * @param timestamp true if the frame should be timestamped by hardware
     *        @note Has much higher precision, but receive needs to be called before another message arrives.
     *        Otherwise it will not timestamp the following message.
     * @param high_prio true if the frame should throw high priority interrupt
     * @note Throw bsod on hardware error.
     */
    void set_filter(uint32_t index, const CanardFilter &filter, bool timestamp, bool high_prio) override;

    /**
     * @brief Get error statistics.
     * @note Reading clears err_log counter.
     * @return error statistics
     */
    ErrorStats get_error_stats() override;

    /// ------------------
    /// Interrupt handlers
    /// @note Public, but handled internally, so do not use.
    /// ------------------

    /**
     * @brief Get driver that uses this HAL instance.
     * @param hfdcan_isr HAL instance
     * @return driver
     */
    static FdcanDriver &get_driver(FDCAN_HandleTypeDef *hfdcan_isr);

    /**
     * @brief Tx Event callback HAL_FDCAN_TxEventFifoCallback().
     * @param  time_isr timestamp of the interrupt
     * @param  TxEventFifoITs indicates which Tx Event FIFO interrupts are signalled
     */
    void tx_event_callback(int64_t time_isr, uint32_t TxEventFifoITs);

    /**
     * @brief  Rx FIFO 1 callback HAL_FDCAN_RxFifo1Callback().
     * @param  time_isr timestamp of the interrupt
     */
    void rx_fifo1_sample_time(int64_t time_isr);
};

} // namespace can
