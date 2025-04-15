#pragma once

#include <canard.h>

#include <functional>
#include <optional>

namespace can {

/**
 * @brief Interface for CAN driver.
 */
class Driver {
public:
    /// Events that can be notified to the application.
    enum class Notification {
        TxDone, ///< Tx buffer is freed and could be used to send another frame
        RxDone, ///< Frame Rx, pick up the frame by receive()
        RxHighPrio, ///< High priority frame Rx, as configured by set_filter(), pick up the frame by receive()
        RxLost, ///< Didn't call receive() often enough, frame was lost
    };

    /**
     * @brief Callback type when event happens.
     * @param notification which event happened
     */
    using NotifyCallback = std::function<void(Notification notification)>;

private:
    /// Notify the application about events. Called from ISR or task.
    NotifyCallback notify_callback = nullptr;

public:
    virtual ~Driver() = default;

    /**
     * @brief Set callback for ISR notifications.
     * Intended to be set from the driver user.
     * @param callback callback to be called from ISR or task
     */
    inline void set_notify_callback(NotifyCallback callback) {
        notify_callback = callback;
    }

    /**
     * @brief Do last minute inits and start CAN driver to be active on the bus.
     * @param automatic_retransmission_enable true to enable automatic retransmission after Tx failure, false to disable
     * @note Throw bsod on hardware error.
     */
    virtual void start(bool automatic_retransmission_enable = true) = 0;

    /**
     * @brief Enable or disable retransmission after failed Tx.
     * @param enable true to retransmit until successful, false to send once and give up
     */
    virtual void set_automatic_retransmission(bool enable) = 0;

    /**
     * @brief Get CAN frame and put it to hardware Tx queue.
     * @param frame CAN frame
     * @param timestamp true if the frame should be timestamped
     * @return true on success, false if there is no space in the queue
     * @note Throw bsod on hardware error.
     * @note Functions send() and get_sent_timestamp() should be called from the same thread.
     */
    virtual bool send(const CanardFrame &frame, bool store_timestamp = false) = 0;

    /**
     * @brief Get timestamp of last frame that had store_timestamp set to true.
     * @return timestamp of the last frame or 0 if the Tx event was lost, nullopt if not yet sent
     * @note Functions send() and get_sent_timestamp() should be called from the same thread.
     */
    virtual std::optional<CanardMicrosecond> get_sent_timestamp() = 0;

    /**
     * @brief Get CAN frame from hardware Rx queue.
     * @param[out] frame CAN frame
     * @warning Buffer for frame.payload is overwritten when this function is called second time.
     * @param[out] timestamp_us timestamp of the received frame
     * @return true on success, false if there is no frame in the queue
     * @note Throw bsod on hardware error.
     */
    virtual bool receive(CanardFrame &frame, CanardMicrosecond *timestamp_us = nullptr) = 0;

    /**
     * @brief Get number of available filters.
     * @return number of indexes that can be used in set_filter()
     */
    virtual uint32_t filter_count() const = 0;

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
    virtual void set_filter(uint32_t index, const CanardFilter &filter, bool timestamp, bool high_prio) = 0;

    /// ------------------
    /// Interrupt handlers
    /// @note Public, but handled internally, so do not use.
    /// ------------------

    /**
     * @brief Notify the application about events. Called from ISR or task.
     * Public, but intended to be called only from driver implementation.
     * @param notification which event happened
     */
    inline void isr_notify(Notification notification) {
        if (notify_callback) {
            notify_callback(notification);
        }
    }
};

} // namespace can
