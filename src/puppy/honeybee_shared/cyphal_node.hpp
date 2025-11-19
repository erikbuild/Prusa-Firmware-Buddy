#pragma once

#include <cyphal_sender_data.hpp>
#include <cyphal_suber_data.hpp>
#include <cyphal_client.hpp>
#include <cyphal_server.hpp>
#include <cyphal_register_dummy.hpp>
#include <cyphal_register.hpp>
#include <cyphal_timesync.hpp>
#include <cyphal_port_list.hpp>
#include <cyphal_task.hpp>
#include <cyphal_pnp.hpp>

#include <uavcan/node/Heartbeat_1_0.h>
#include <uavcan/node/GetInfo_1_0.h>
#include <uavcan/node/ExecuteCommand_1_3.h>

#include <option/has_cyphal_metrics.h>
#if HAS_CYPHAL_METRICS()
    #include <prusa3d/common/metrics/Broadcast_1_0.h>
    #include <prusa3d/common/PortIds_0_1.h>
    #include <puppy_metrics.hpp>
#endif

#include <option/cyphal_can_stats.h>
#if CYPHAL_CAN_STATS()
    #include <cyphal_can_stats.hpp>
#endif

#include <option/has_cyphal_logging.h>
#if HAS_CYPHAL_LOGGING()
    #include <cyphal_record.hpp>
#endif

#include <prusa3d/common/CustomExecuteCommand_1_0.h>
#include <watcher.hpp>

#include <salted_app_hash_command_server.hpp>

#include <safe_state.h>
#include <timing.h>
#include <utility>
#include <device/cmsis.h>

LOG_COMPONENT_REF(Node);

namespace can::cyphal {

namespace detail {
    // Optional PnP class
    template <CanardNodeID node_id>
    struct OptionalPnP {
        OptionalPnP([[maybe_unused]] CanardNodeID request_id,
            [[maybe_unused]] const uint8_t uid[sizeof(uavcan_pnp_NodeIDAllocationData_2_0::unique_id)]) {}
        static constexpr bool has_pnp = false;
    };
    template <>
    struct OptionalPnP<CANARD_NODE_ID_UNSET> {
        OptionalPnP(CanardNodeID request_id, const uint8_t uid[sizeof(uavcan_pnp_NodeIDAllocationData_2_0::unique_id)])
            : pnp(request_id, uid) {} ///< Initialize PnP with request ID and unique ID
        PnP pnp; ///< PnP that will request node ID
        static constexpr bool has_pnp = true;
    };

    /// @brief Parts of Node that can be untemplated and hidden.
    struct ProtoNode {
        /// Add otp registers
        static void add_otp_registers(RegisterMachineIface &registers);

        /**
         * @brief Init getinfo structure.
         * @param get_info_resp GetInfo response to fill
         * @param name name of the node, will be copied to get_info_resp.name
         * @param uid unique ID of the node, will be copied to get_info_resp.unique_id
         */
        static void init_info(uavcan_node_GetInfo_Response_1_0 &get_info_resp,
            const char *name, const uint8_t uid[sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)]);

        /// Handle Execute Command request for reset
        static uint8_t handle_execute_command_reset();

        /// Handle Execute Command request for emergency stop
        static uint8_t handle_execute_command_emergency_stop();
    };
} // namespace detail

namespace defaults {
    /**
     * @brief Nodes whose presence is watched.
     */
    enum class WatchNodes : size_t {
        info_requester, ///< Watch the node that requests our info
        config_setter, ///< Watch the node that sets our config
        id_giver, ///< Watch the node that gives us our ID (only for PnP nodes)
        _count, ///< Number of watched nodes
    };

    /**
     * @brief List of notifications.
     * This needs to be bitmask, each notify its own bit.
     */
    enum class Notify : uint32_t {
        status = 1 << 0, ///< Send status immediately
        heartbeat = 1 << 1, ///< Send heartbeat immediately
        debug_set = 1 << 2, ///< Debug configuration changed, use is_debug_enabled() to check
        time_sync = 1 << 3, ///< Time sync changed
        operational = 1 << 4, ///< Switched to operational mode
    };
} // namespace defaults

/**
 * @brief Cyphal node common stuff that honeybee node should have.
 *
 * @tparam StatusTraits periodically sent status message
 * @tparam status_port_id port ID for status message
 * @note Default values are not set. All are inited to {}. It should be valid data.
 * @note Status needs to have:
 *     bool time_sync_precise
 *     prusa3d.common.SharedFault.1.0 faults
 *
 * @tparam ConfigTraits config that can be written by a service
 * @tparam config_port_id port ID for config service
 *
 * @tparam SpecificFault bitmask of faults used to report faults in status
 *
 * @tparam node_id node ID, if set to CANARD_NODE_ID_UNSET, PnP will be used to get the node ID
 * @tparam WatchNodes nodes whose presence is watched, defaults to defaults::WatchNodes
 * @tparam Notify notification types, defaults to defaults::Notify
 * @tparam node_id_request ask for this node ID in case of PnP
 * @tparam MAX_REGISTERS maximum number of registers
 */
template <
    typename StatusTraits_, CanardPortID status_port_id,
    typename ConfigTraits_, CanardPortID config_port_id,
    typename SpecificFault_,
    CanardNodeID node_id = CANARD_NODE_ID_UNSET,
    typename WatchNodes_ = defaults::WatchNodes,
    typename Notify_ = defaults::Notify,
    CanardNodeID node_id_request = 2,
    size_t MAX_REGISTERS = 40>
class Node : public detail::OptionalPnP<node_id> {
public:
    /// Link the types so they are accessible from children
    using StatusTraits = StatusTraits_;
    using ConfigTraits = ConfigTraits_;
    using SpecificFault = SpecificFault_;
    using WatchNodes = WatchNodes_;
    using Notify = Notify_;

private:
    TaskHandle_t notify_task = nullptr; ///< Loop task to wake up on CAN event
protected:
    static constexpr UBaseType_t notify_index = 1; ///< Wake up with an index (should not be 0)

    /**
     * @brief Notify the loop() task to wake up.
     * Can be called from ISR.
     * @param what which notification
     */
    void notify(std::convertible_to<Notify> auto &&...what) {
        if (notify_task == nullptr) {
            return;
        }
        uint32_t what_flags = 0;
        for (auto w : std::initializer_list<Notify> { what... }) {
            what_flags |= std::to_underlying(w);
        }

        if (__get_IPSR() != 0) { // We are in ISR
            BaseType_t woken = pdFALSE;
            xTaskNotifyIndexedFromISR(notify_task, notify_index, what_flags, eSetBits, &woken);
            portYIELD_FROM_ISR(woken);
        } else {
            xTaskNotifyIndexed(notify_task, notify_index, what_flags, eSetBits);
        }
    }

    /// Time synchronization object
    can::cyphal::TimeSync time_sync;

    /// Heartbeat message, mandatory state report
    can::cyphal::SenderDirectTraited<uavcan_node_Heartbeat_1_0_Traits> heartbeat;
    uavcan_node_Mode_1_0 mode = { .value = uavcan_node_Mode_1_0_INITIALIZATION }; ///< Start in init and switch to operational when we have time sync and config
    std::atomic<bool> first_config = false; ///< True when we have first config
    bool first_time_sync = false; ///< True when we have first time sync
    bool is_time_sync_precise = false;
    int64_t last_heartbeat = 0; ///< Last time we sent heartbeat

    ///< Publish list of used ports
    can::cyphal::PortList port_list;

    /// ExecuteCommand server, mandatory command interface
    can::cyphal::ServerTraited<uavcan_node_ExecuteCommand_1_3_Traits> execute_command_server;
    SaltedAppHashCommandServer hasher; ///< To respond to app hash requests

    /// Mandatory register interface
    can::cyphal::RegisterMachine<MAX_REGISTERS> registers;
    std::atomic<bool> debug_enable = false; ///< Register to enable debug stuff

    /// GetInfo Server, mandatory device info
    can::cyphal::ServerTraited<uavcan_node_GetInfo_1_0_Traits> get_info_server;
    uavcan_node_GetInfo_Response_1_0 get_info_resp;

#if HAS_CYPHAL_LOGGING()
    /// Logging destination
    can::cyphal::Record record;
#endif

    /// Heartbeat subscription to watch if other nodes are alive
    watcher::Heartbeat<WatchNodes> heartbeat_watcher;

    /// Config server
    can::cyphal::ServerTraited<ConfigTraits, config_port_id> config_server;

    int64_t last_status_timestamp = 0; ///< Last local time we sent status [us]
    static constexpr int64_t status_send_interval = 250'000; ///< Interval for sending status [us]

    std::atomic<uint32_t> fault_mask = 0; ///< Faults to be sent in status
    std::atomic<bool> advisory = false; ///< Send advisory health if set by set_advisory()

public:
#if HAS_CYPHAL_METRICS()
    /// Metrics sender
    can::cyphal::SenderDirectTraited<prusa3d_common_metrics_Broadcast_1_0_Traits,
        prusa3d_common_PortIds_0_1_MSG_COMMON_METRICS>
        metrics_sender;
#endif

#if CYPHAL_CAN_STATS()
    /// Bus communication statistics
    can::cyphal::CanStats can_stats;
#endif

    /**
     * @brief Sends status message.
     * You can push data directly into it.
     * @note Do not set significant mark. It is handled from here.
     * Use send_status() instead and it will update other sensors before send.
     */
    can::cyphal::SenderDataTraited<StatusTraits, status_port_id> status_sender;

    /**
     * @brief Construct Node.
     * @param uid unique ID of the node, must be 16 bytes long
     * @param name name of the node as in "cz.prusa3d.honeybee.hub"
     */
    Node(const uint8_t uid[sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)], const char *name)
        : detail::OptionalPnP<node_id>(node_id_request, uid)
        , time_sync(0)
        , execute_command_server(
              [this](const uavcan_node_ExecuteCommand_Request_1_3 &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                  uavcan_node_ExecuteCommand_Response_1_3 resp = { .status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_BAD_COMMAND, .output = {} };
                  switch (data.command) {
                  case uavcan_node_ExecuteCommand_Request_1_3_COMMAND_RESTART:
                      resp.status = detail::ProtoNode::handle_execute_command_reset();
                      break;
                  case uavcan_node_ExecuteCommand_Request_1_3_COMMAND_EMERGENCY_STOP:
                      resp.status = detail::ProtoNode::handle_execute_command_emergency_stop();
                      break;
                  case prusa3d_common_CustomExecuteCommand_1_0_COMMAND_GET_APP_SALTED_HASH:
                      hasher.handle_request(execute_command_server, data, resp);
                      return; // Response was either already sent or will be sent later
#if CYPHAL_CAN_STATS()
                  case prusa3d_common_CustomExecuteCommand_1_0_COMMAND_START_CAN_STAT_TX:
                      if (can_stats.start_tx_session(data.parameter.count, data.parameter.elements)) {
                          resp.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_SUCCESS;
                      } else {
                          resp.status = uavcan_node_ExecuteCommand_Response_1_3_STATUS_BAD_PARAMETER;
                      }
                      break;
#endif // CYPHAL_CAN_STATS()
                  default:
                      if (!execute_command(data, resp)) {
                          return; // App will send response later
                      }
                  }
                  execute_command_server.send_response(resp);
              },
              ProtoSender::send_timeout_default, ProtoSuber::multipart_timeout_default)
        , get_info_server(
              [this]([[maybe_unused]] const uavcan_node_GetInfo_Request_1_0 &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                  // Watch the node that requests our info first (can be Yukon, but little risk for user)
                  heartbeat_watcher.register_node_id(WatchNodes::info_requester, meta.remote_node_id, meta.timestamp, false);
                  get_info_server.send_response(get_info_resp);
              },
              ProtoSender::send_timeout_default, ProtoSuber::multipart_timeout_default)
        , config_server(
              [this](const ConfigTraits::Request::Type &config, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                  // Watch the node that sets our config
                  heartbeat_watcher.register_node_id(WatchNodes::config_setter, meta.remote_node_id, meta.timestamp);

                  write_config(config);

                  first_config.store(true);
                  config_server.send_response(typename ConfigTraits::Response::Type()); // Dummy response, no data
              },
              ProtoSender::send_timeout_default, ProtoSuber::multipart_timeout_short) {
        detail::ProtoNode::init_info(get_info_resp, name, uid);
    }

    /**
     * @brief Initialize CAN node.
     * This should be directly called during application initialization.
     * Specific init is handled in app_init() method.
     */
    void init() {
        // Add registers
        detail::ProtoNode::add_otp_registers(registers);

        registers.add_register(
            "debug_enable",
            [this](const uavcan_register_Value_1_0 &write, uavcan_register_Value_1_0 &read) {
                if (uavcan_register_Value_1_0_is_bit_(&write) && write.bit.value.count == 1) {
                    debug_enable.store(write.bit.value.bitpacked[0] & 0x1);
                    notify(Notify::debug_set); // Notify app that debug state changed
                }
                uavcan_register_Value_1_0_select_bit_(&read);
                read.bit.value.bitpacked[0] = debug_enable.load() ? 0x1 : 0x00;
                read.bit.value.count = 1;
            },
            true);

        // Init sub components
        port_list.init();
        time_sync.init(port_list);
#if HAS_CYPHAL_LOGGING()
        record.init(port_list);
#endif
        registers.init(port_list);
#if CYPHAL_CAN_STATS()
        can_stats.init(port_list, registers);
#endif
        heartbeat_watcher.init(port_list);

        // Add subscriptions and publishers
        execute_command_server.add_to_task();
        get_info_server.add_to_task();
        status_sender.add_to_task();
        config_server.add_to_task();

        port_list.add(heartbeat);
        port_list.add(execute_command_server);
        port_list.add(get_info_server);
#if HAS_CYPHAL_METRICS()
        port_list.add(metrics_sender);
#endif
        port_list.add(status_sender);
        port_list.add(config_server);

        // Add port names and types
        registers.add_port_name_set("srv.set_config", ConfigTraits::full_name_and_version, config_server.get_server_id());
        registers.add_port_name_set("pub.status", StatusTraits::full_name_and_version, status_sender.get_port_id());
#if HAS_CYPHAL_METRICS()
        registers.add_port_name_set("pub.metrics", prusa3d_common_metrics_Broadcast_1_0_Traits::full_name_and_version, metrics_sender.get_port_id());
#endif

        // Initialize registers, components and ports for the child app
        app_init();

        if constexpr (this->has_pnp) {
            this->pnp.start(); // Start PnP process
        } else {
            cyphal_task.set_node_id(node_id); // Set fixed node ID
        }
    }

    /// Task function (never exits)
    [[noreturn]] static void task(void *argument) {
        assert(argument != nullptr);
        reinterpret_cast<Node *>(argument)->task();
    }

    /// Task function (never exits)
    [[noreturn]] void task() {
        notify_task = xTaskGetCurrentTaskHandle(); // Send notification to this task
        device::MultiWatchdog cyphal_app_watchdog;

        while (true) {
            int64_t now = get_timestamp_us(); ///< Get time only once per loop

            if constexpr (this->has_pnp) {
                // If anonymous, handle only PnP
                if (cyphal_task.is_anonymous()) {
                    this->pnp.loop_tx();
                    app_tick_pnp(now);
                    vTaskDelay(pdMS_TO_TICKS(1));
                    cyphal_app_watchdog.kick();
                    continue;
                } else if (heartbeat_watcher.get_node_id(WatchNodes::id_giver) == CANARD_NODE_ID_UNSET) {
                    // Watch the node that gave us our ID
                    heartbeat_watcher.register_node_id(WatchNodes::id_giver, this->pnp.get_id_giver(), get_timestamp_us());
                }
            }

#if HAS_CYPHAL_LOGGING()
            // Logging
            record.try_send();
#endif

            // Time synchronization
            time_sync.loop(now);
            const bool new_is_time_sync_precise = time_sync.is_precise();
            if (is_time_sync_precise != new_is_time_sync_precise) {
                status_sender.transform_data(
                    [new_is_time_sync_precise](StatusTraits::Type &status) -> TransformResult {
                        status.time_sync_precise = new_is_time_sync_precise;
                        return { true, false };
                    });
                last_status_timestamp = 0; // Send status immediately

                if (!first_time_sync) {
                    assert(new_is_time_sync_precise);
                    log_info(Node, "Time synchronized with master");

                } else if (new_is_time_sync_precise) {
                    log_info(Node, "Time sync is precise");
                } else {
                    log_warning(Node, "Time sync precision lost");
                }

                is_time_sync_precise = new_is_time_sync_precise;
                notify(Notify::time_sync); // Notify app that time sync changed
            }
            if (!first_time_sync && new_is_time_sync_precise) {
#if HAS_CYPHAL_LOGGING()
                // Got first valid sync during initialization
                record.set_time_sync(&time_sync); // Use time_sync for log timestamps
#endif
                registers.set_time_sync(&time_sync); // Use time_sync for register timestamps
                first_time_sync = true;
            }

            // Handle requests for firmware hash
            hasher.step(execute_command_server);

            if (first_config.load()) {
                // Watch the presence of Buddy, Beenix, but only after config is received.
                // Before config, nodes might restart as they please, so we need to wait for stable state.
                heartbeat_watcher.check(now);
            }

            // Send status (if we have fault triggered from interrupt or periodically)
            if (now - last_status_timestamp > status_send_interval) {
                last_status_timestamp = now;

                // Gather data
                status_sender.transform_data(
                    [this](StatusTraits::Type &data) -> TransformResult {
                        data.faults.bitmask = fault_mask.load(); // Faults
                        update_status(data);
                        return { .success = true, .significant = true }; // Send now
                    });
            }

#if HAS_CYPHAL_METRICS()
            // Record metrics
            metrics::record_puppy_system();
#endif

            // Handle specific node app stuff
            app_tick(now);

#if CYPHAL_CAN_STATS()
            // Print results of CAN stats
            can_stats.poll_finished_stats(now);
#endif

            // Send own heartbeat
            if (mode.value == uavcan_node_Mode_1_0_INITIALIZATION
                // Switch from init to operational mode
                && first_time_sync && first_config.load()) {
                mode.value = uavcan_node_Mode_1_0_OPERATIONAL;
                last_heartbeat = 0; // Send heartbeat immediately
                notify(Notify::operational); // Notify app that we are operational
                log_info(Node, "Switched to operational mode");
            }
            if (now - last_heartbeat > uavcan_node_Heartbeat_1_0_MAX_PUBLICATION_PERIOD * 1'000'000) {
                uavcan_node_Heartbeat_1_0 hb = {
                    .uptime = static_cast<uint32_t>(now / 1'000'000),
                    .health = { .value = uavcan_node_Health_1_0_NOMINAL },
                    .mode = mode,
                    /// @todo We could use status code to report something.
                    .vendor_specific_status_code = 0,
                };
                if (fault_mask.load() != 0) {
                    hb.health = { .value = uavcan_node_Health_1_0_WARNING }; // Some fault
                } else if (advisory.load()) {
                    hb.health = { .value = uavcan_node_Health_1_0_ADVISORY }; // Some advisory state
                }
                heartbeat.send_data(hb);
                last_heartbeat = now;
            }

            // Sleep
            uint32_t what;
            if (xTaskNotifyWaitIndexed(notify_index, 0, 0xffffffff, &what, pdMS_TO_TICKS(1)) == pdPASS) {
                // Send status
                if (what & std::to_underlying(Notify::status)) {
                    last_status_timestamp = 0;
                }

                // Send heartbeat immediately
                if (what & std::to_underlying(Notify::heartbeat)) {
                    last_heartbeat = 0;
                }

                // Call app specific notify handlers
                app_notify(what);
            }

            cyphal_app_watchdog.kick();
        }
    }

protected:
    /// @brief Initialize app part of the node.
    virtual void app_init() = 0;

    /**
     * @brief Handle specific node app stuff.
     * @param now current time in microseconds
     * Record additional metrics.
     * Read some sensors.
     */
    virtual void app_tick(int64_t now) = 0;

    /**
     * @brief Handle specific node stuff while waiting for the PnP
     * @param now current time in microseconds
     */
    virtual void app_tick_pnp([[maybe_unused]] int64_t now) {}

    /**
     * @brief Notification from notify().
     * @param what bitmask with Notify flags
     * Called from the same task as app_tick(), but wakes up immediately.
     */
    virtual void app_notify([[maybe_unused]] uint32_t what) {};

    /**
     * @brief Handle additional ExecuteCommand requests.
     * @warning Do not use this for heavy calculations, this is called from CAN thread.
     * @param request request data
     * @param response response data to send if return true
     * @return true to send response, false to let app handle it
     */
    virtual bool execute_command([[maybe_unused]] const uavcan_node_ExecuteCommand_Request_1_3 &request,
        [[maybe_unused]] uavcan_node_ExecuteCommand_Response_1_3 &response) {
        return true; // Send bad command response
    }

    /**
     * @brief Store new received config.
     * @warning No heavy computations here, this is called from CAN thread.
     * @param config incomming configuration structure
     */
    virtual void write_config(const ConfigTraits::Request::Type &config) = 0;

    /**
     * @brief Update sensors before being sent.
     * Sensors can also directly write to status_sender.
     */
    virtual void update_status(StatusTraits::Type &data) = 0;

public:
    /// @brief Update status structure and send it immediatelly.
    void send_status() {
        notify(Notify::status);
    }

    /// @brief Get synchronization with network time.
    TimeSync &get_timesync() {
        return time_sync;
    }

    /// @brief Set or clear advisory health state.
    void set_advisory(bool advisory_) {
        advisory.store(advisory_);
    }

    /// @brief Get debug state set by a Cyphal register.
    bool is_debug_enabled() const {
        return debug_enable.load();
    }

    /**
     * @brief Set fault to be sent in status.
     * This can be used from ISR.
     * When set, health will change to WARNING, disregarding if advisory is set.
     * @note This just sets fault to cyphal register, no other action is taken.
     *       To trigger fault of the system, use hub::trigger_fault().
     */
    void set_fault(SpecificFault fault) {
        fault_mask |= (1 << fault);
        notify(Notify::status, Notify::heartbeat); // Send status and heartbeat immediately
    }
};

} // namespace can::cyphal
