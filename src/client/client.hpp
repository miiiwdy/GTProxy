#pragma once
#include "enet_wrapper.hpp"
#include "../core/core.hpp"
#include "../packet/packet_types.hpp"

namespace client {
class Client final : public ENetWrapper {
public:
    explicit Client(core::Core* core);
    ~Client();

    void process() override;

    void on_connect(ENetPeer* peer) override;
    void on_receive(ENetPeer* peer, ENetPacket* packet) override;
    void on_disconnect(ENetPeer* peer) override;

private:
    core::Core* core_;
};
}