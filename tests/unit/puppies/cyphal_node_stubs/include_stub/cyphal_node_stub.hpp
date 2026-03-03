#pragma once

#include <catch2/catch_test_macros.hpp>

#include <prusa3d/common/PortIds_0_1.h>
#include <prusa3d/nfc/SetConfig_1_0.h>
#include <prusa3d/nfc/Status_1_0.h>

#include <cyphal_node.hpp>

enum Fault {};

using NodeParent = can::cyphal::Node<
    prusa3d_nfc_Status_1_0_Traits, prusa3d_common_PortIds_0_1_MSG_NFC_STATUS,
    prusa3d_nfc_SetConfig_1_0_Traits, prusa3d_common_PortIds_0_1_SRV_NFC_SET_CONFIG,
    Fault>;

static constexpr uint8_t uid[sizeof(uavcan_node_GetInfo_Response_1_0::unique_id)] {};

class Node : public NodeParent {

public:
    Node()
        : NodeParent(uid, "nodename") {}

    void app_init() override {}
    void app_tick(int64_t) override {}
    void app_notify(uint32_t) override {}

    void write_config(const ConfigTraits::Request::Type &) override {}
    void update_status(StatusTraits::Type &) override {}
};

class NoDriver final : public can::Driver {

public:
    void start(bool) override {
    }

    void set_automatic_retransmission(bool) override {
    }

    bool send(const CanardFrame &, bool) override {
        return false;
    }

    std::optional<CanardMicrosecond> get_sent_timestamp() override {
        return {};
    }

    bool receive(CanardFrame &, std::array<uint8_t, CANARD_MTU_CAN_FD> &, CanardMicrosecond *) override {
        return false;
    }

    uint32_t filter_count() const override {
        return 0;
    }

    void set_filter(uint32_t, const CanardFilter &, bool, bool) override {
    }

    uint32_t get_error_log() const override {
        return 0;
    }
};
