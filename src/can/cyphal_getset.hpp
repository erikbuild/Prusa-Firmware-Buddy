#pragma once

#include <canard.h>
#include "cyphal_server.hpp"
#include "cyphal_task.hpp"

namespace can::cyphal {

/**
 * @brief Two servers to provide "get" and "set" functions of a config.
 * This is to be used on the device being configured.
 * All parameters are transpiled from DSDL and found inside three files Config, GetConfig and SetConfig.
 *
 * @tparam T_CONFIG config type, looks like "module_submodule_ConfigType_1_0"
 * @note The "get" server is to be used with a service that has an empty request and only T_CONFIG named "config" in response.
 * @note The "set" server is to be used with a service that has only T_CONFIG named "config" in request and an empty response.
 *
 * Get request
 * @tparam T_GET_REQUEST data type, looks like "module_submodule_GetConfig_Request_1_0"
 * @tparam EXTENT_GET_REQUEST size of the buffer to receive serialized data, looks "like module_submodule_GetConfig_Request_1_0_EXTENT_BYTES_"
 * @note There is no function to deserialize the data, the type needs to be empty.
 *
 * Get response
 * @tparam T_RESPONSE data type, looks like "module_submodule_GetConfig_Response_1_0"
 * @tparam SIZE_RESPONSE size of the buffer to hold serialized data, looks "like module_submodule_GetConfig_Response_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_"
 * @note Constructor parameter "serialize_fn" function to serialize the data, looks like "module_submodule_ConfigType_1_0_serialize_".
 *
 * Set request
 * @tparam T_REQUEST data type, looks like "module_submodule_SetConfig_Request_1_0"
 * @tparam EXTENT_REQUEST size of the buffer to receive serialized data, looks "like module_submodule_SetConfig_Request_1_0_EXTENT_BYTES_"
 * @note Constructor parameter "deserialize_fn": function to deserialize the data, looks like "module_submodule_ConfigType_1_0_deserialize_".
 *
 * Set response
 * @tparam T_RESPONSE data type, looks like "module_submodule_SetConfig_Response_1_0"
 * @tparam SIZE_RESPONSE size of the buffer to hold serialized data, looks "like module_submodule_SetConfig_Response_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_"
 * @note There is no function to serialize the data, the type needs to be empty.
 */
template <typename T_CONFIG,
    typename T_GET_REQUEST, size_t EXTENT_GET_REQUEST,
    typename T_GET_RESPONSE, size_t SIZE_GET_RESPONSE,
    typename T_SET_REQUEST, size_t EXTENT_SET_REQUEST,
    typename T_SET_RESPONSE, size_t SIZE_SET_RESPONSE>
class GetSetServer {
    static_assert(sizeof(T_GET_REQUEST) == 1, "Needs to be dummy request!");
    static_assert(EXTENT_GET_REQUEST == 0, "Needs to be dummy request!");

    static_assert(sizeof(T_CONFIG) == sizeof(T_GET_RESPONSE), "These need to be equal!");
    static_assert(std::is_same<T_CONFIG, decltype(T_GET_RESPONSE::config)>::value, "These need to be equal!");

    static_assert(sizeof(T_CONFIG) == sizeof(T_SET_REQUEST), "These need to be equal!");
    static_assert(std::is_same<T_CONFIG, decltype(T_SET_REQUEST::config)>::value, "These need to be equal!");

    static_assert(sizeof(T_SET_RESPONSE) == 1, "Needs to be dummy response!");
    static_assert(SIZE_SET_RESPONSE == 0, "Needs to be dummy response!");

public:
    /**
     * @brief Callback type when config is "set".
     * @note It is called from the CAN thread. Do not use this callback for heavy processing.
     * @param config set config
     * @param meta metadata of the received request
     */
    using Callback = std::function<void(const T_CONFIG &config, const ProtoSuber::Meta &meta)>;

private:
    T_CONFIG config; ///< Hold the config

    Server<uint8_t, 0, T_CONFIG, SIZE_GET_RESPONSE> get_server; ///< Server with empty request and only T_CONFIG in response
    Server<T_CONFIG, EXTENT_SET_REQUEST, uint8_t, 0> set_server; ///< Server with only T_CONFIG in request and an empty response

    Callback callback; ///< Callback on config "set"

public:
    /**
     * @brief Double-server object that is used to configure a device.
     * @note After creation, you must call add_to_task() to add itself to Cyphal Task.
     *
     * @param config_ initial default config
     *
     * @param deserialize_fn function to deserialize the "set" config, looks like "module_submodule_ConfigType_1_0_deserialize_"
     * @param serialize_fn function to serialize the "get" config, looks like "module_submodule_ConfigType_1_0_serialize_"
     *
     * @param get_port_id Cyphal GetConfig service port-ID
     * @param set_port_id Cyphal SetConfig service port-ID
     * @param callback_ callback to be called when config is "set"
     *    @note Callback is called from the CAN thread. Do not use this callback for heavy processing.
     *
     * @param send_timeout timeout to transmit response, see Server for more info
     * @param multipart_timeout timeout for request, see Server for more info
     */
    GetSetServer(const T_CONFIG &config_,
        SuberCall<T_CONFIG, EXTENT_SET_REQUEST>::DeserializeFn &deserialize_fn, SenderDirect<T_CONFIG, SIZE_GET_RESPONSE>::SerializeFn &serialize_fn,
        CanardPortID get_port_id, CanardPortID set_port_id,
        const Callback callback_,
        CanardMicrosecond send_timeout, CanardMicrosecond multipart_timeout)
        : config(config_)
        , get_server(
              ProtoSuber::dummy_deserialize, serialize_fn, get_port_id,
              [this]([[maybe_unused]] const uint8_t &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                  get_server.send_response(config);
              },
              send_timeout, multipart_timeout)
        , set_server(
              deserialize_fn, ProtoSender::dummy_serialize, set_port_id,
              [this](const T_CONFIG &data, [[maybe_unused]] const ProtoSuber::Meta &meta) {
                  config = data;
                  set_server.send_response(0); // Response with nothing
                  callback(config, meta);
              },
              send_timeout, multipart_timeout)
        , callback(callback_) {
        assert(callback);
    }

    /**
     * @brief Get local config.
     * @param mutex_timeout timeout to wait for mutex lock
     * @return config or nullopt if timeout
     */
    [[nodiscard]] std::optional<T_CONFIG> get_config_timeout(TickType_t mutex_timeout) {
        auto lock = cyphal_task.lock_subers_mutex(mutex_timeout);
        if (lock.is_locked() == false) {
            return std::nullopt;
        }
        return config;
    }

    /**
     * @brief Get local config.
     * @return config
     */
    [[nodiscard]] T_CONFIG get_config() {
        auto lock = cyphal_task.lock_subers_mutex();
        assert(lock.is_locked());
        return config;
    }

    /**
     * @brief Set local config.
     * @note The other side will not know of the change until it "get"s the config.
     * @param config new config
     * @param mutex_timeout timeout to wait for mutex lock
     * @return true if set, false if mutex timeout
     */
    bool set_config(const T_CONFIG &config_, TickType_t mutex_timeout = portMAX_DELAY) {
        auto lock = cyphal_task.lock_subers_mutex(mutex_timeout);
        if (lock.is_locked() == false) {
            return false;
        }

        config = config_;

        return true;
    }

    /**
     * @brief Transform local config.
     * @note The other side will not know of the change until it "get"s the config.
     * @param transformer use this function to modify config
     * @param mutex_timeout timeout to wait for mutex lock
     * @return return from transformer if applied, false if mutex timeout
     */
    [[nodiscard]] bool transform_config(std::function<bool(T_CONFIG &config)> transformer, TickType_t mutex_timeout = portMAX_DELAY) {
        auto lock = cyphal_task.lock_subers_mutex(mutex_timeout);
        if (lock.is_locked() == false) {
            return false;
        }

        bool ret = transformer(config);

        return ret;
    }

    /// @brief Add itself to Cyphal Task.
    void add_to_task() {
        get_server.add_to_task();
        set_server.add_to_task();
    }

    /// @brief Remove itself from Cyphal Task.
    void remove_from_task() {
        get_server.remove_from_task();
        set_server.remove_from_task();
    }

    /// @brief Server port-IDs.
    struct ServerIds {
        CanardPortID get; ///< get_server's port-ID
        CanardPortID set; ///< set_server's port-ID
    };

    /// @brief Get server port-IDs.
    [[nodiscard]] ServerIds get_server_ids() const {
        return { .get = get_server.get_server_id(), .set = set_server.get_server_id() };
    }

    /// @return Get request sender for PortList.
    ProtoPortList &get_get_protoportlist() {
        return get_server.get_protoportlist();
    }

    /// @return Set request sender for PortList.
    ProtoPortList &get_set_protoportlist() {
        return set_server.get_protoportlist();
    }
};

} // namespace can::cyphal
