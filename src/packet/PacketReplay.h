#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "../wow/WowTypes.h"

// ============================================================
//  PacketReplay — send stored packets back to the server
//
//  Only CMSG packets can be meaningfully replayed (we re-send
//  them on behalf of the client).  SMSG replay (injecting fake
//  server packets locally) is a future TODO.
//
//  The caller must set the send function pointer before use.
//  Set it in PacketHooks once WowConnection::Send is found.
// ============================================================

// Game signature: int __thiscall WowConnection::Send(WowConnection* this, CDataStore* packet, int priority)
using fn_WowConn_Send = int(__thiscall*)(WowConnection*, CDataStore*, int);

class PacketReplay
{
public:
    // Must be called with the real WowConnection::Send address before replaying.
    static void SetSendFn(fn_WowConn_Send fn, WowConnection* conn);

    // Build a raw packet buffer from an opcode + payload and transmit it.
    // Returns false if send function is not set or transmission fails.
    static bool Send(uint16_t opcode, const std::vector<uint8_t>& payload);

    // Replay a previously captured packet (CMSG only).
    static bool ReplayCaptured(const CapturedPacket& pkt);

    // Replay multiple packets in sequence with an optional delay between each (ms).
    static bool ReplaySequence(const std::vector<CapturedPacket>& pkts, uint32_t delayMs = 0);

    static bool IsReady() { return s_sendFn != nullptr && s_conn != nullptr; }

private:
    static inline fn_WowConn_Send  s_sendFn = nullptr;
    static inline WowConnection*   s_conn   = nullptr;
};
