#include "PacketReplay.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>

void PacketReplay::SetSendFn(fn_WowConn_Send fn, WowConnection* conn)
{
    s_sendFn = fn;
    s_conn   = conn;
}

// ============================================================
//  Low-level: build a packet and send via WowConnection::Send.
//
//  The game's Send() expects a CDataStore with exactly:
//    [4 bytes] opcode (uint32 LE, low 2 bytes = opcode)
//    [N bytes] payload
//  Send() itself prepends the 2-byte big-endian size and encrypts.
//  We must NOT include the 2-byte size here or the server sees garbage.
// ============================================================
bool PacketReplay::Send(uint16_t opcode, const std::vector<uint8_t>& payload)
{
    if (!s_sendFn || !s_conn) return false;

    const uint32_t payloadLen = static_cast<uint32_t>(payload.size());

    // Build what the game expects: 4-byte opcode LE + payload only
    std::vector<uint8_t> raw;
    raw.reserve(4 + payloadLen);

    // 4-byte little-endian opcode (low 2 bytes = opcode, high 2 = 0)
    raw.push_back(static_cast<uint8_t>( opcode        & 0xFF));
    raw.push_back(static_cast<uint8_t>((opcode >> 8)  & 0xFF));
    raw.push_back(0x00);
    raw.push_back(0x00);

    raw.insert(raw.end(), payload.begin(), payload.end());

    CDataStore ds = {};
    ds.m_buffer = raw.data();
    ds.m_base   = 0;
    ds.m_alloc  = static_cast<uint32_t>(raw.size());
    ds.m_size   = static_cast<uint32_t>(raw.size());
    ds.m_readPos = 0;

    int result = s_sendFn(s_conn, &ds, 0);
    return result != 0;
}

bool PacketReplay::ReplayCaptured(const CapturedPacket& pkt)
{
    if (pkt.direction != PacketDirection::CMSG) return false;
    return Send(pkt.opcode, pkt.payload);
}

bool PacketReplay::ReplaySequence(const std::vector<CapturedPacket>& pkts, uint32_t delayMs)
{
    bool ok = true;
    for (const auto& pkt : pkts)
    {
        if (pkt.direction != PacketDirection::CMSG) continue;
        if (!ReplayCaptured(pkt)) ok = false;
        if (delayMs > 0) Sleep(delayMs);
    }
    return ok;
}
