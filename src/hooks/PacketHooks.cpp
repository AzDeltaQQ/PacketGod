#include "PacketHooks.h"
#include "HookManager.h"
#include "../wow/Offsets.h"
#include "../wow/WowTypes.h"
#include "../packet/PacketCapture.h"
#include "../packet/PacketReplay.h"
#include "../DebugLog.h"
#include <cstring>
#include <cstdio>

#include <Windows.h>

// Set to 1 to disable the AuthChallenge hook (test if login crash goes away).
// If crash stops when disabled, the handler signature or address may need re-verification.
#ifndef PACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK
#define PACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK 0
#endif

// Best-effort: avoid dereferencing obviously bad pointers (stale/freed).
// IsBadReadPtr is deprecated but still useful for defensive checks here.
static bool IsReadable(const void* ptr, size_t len)
{
    if (!ptr) return false;
    return IsBadReadPtr(ptr, len) == 0;
}

// ============================================================
//  Module-level state
// ============================================================

static WowConnection* s_activeConn = nullptr;

// Captured session key (logged at SetEncryptionKey time)
static uint8_t s_sessionKey[40]  = {};
static uint8_t s_sessionKeyLen   = 0;

// ============================================================
//  Original function trampolines
// ============================================================

// Layer B: ARC4_Process — RE: __cdecl (data, len, srcState, dstState), returns dstState
using fn_ARC4_Process = SARC4State*(__cdecl*)(uint8_t* data, uint32_t len, SARC4State* srcState, SARC4State* dstState);
static fn_ARC4_Process orig_ARC4_Process = nullptr;

// Layer C: WowConnection::SetEncryptionKey
using fn_SetEncKey = void(__thiscall*)(WowConnection*, const uint8_t*, uint8_t, uint8_t, const uint8_t*, uint8_t);
static fn_SetEncKey orig_SetEncKey = nullptr;

// Layer D: NetClient::AuthChallengeHandler
using fn_AuthChallenge = void(__thiscall*)(void*, WowConnection*, CDataStore**);
static fn_AuthChallenge orig_AuthChallenge = nullptr;

// Layer A: WowConnection::Send
using fn_WowConn_Send = int(__thiscall*)(WowConnection*, CDataStore*, int);
static fn_WowConn_Send orig_WowConn_Send = nullptr;

// Wrapper so __thiscall detours are static members (MSVC allows __thiscall only on member functions)
struct Detours
{
    static int __thiscall WowConn_Send(WowConnection* self, CDataStore* packet, int priority);
    static void __thiscall SetEncKey(WowConnection* self, const uint8_t* sessionKey, uint8_t sessionKeyLen,
                                    uint8_t serverMode, const uint8_t* seed, uint8_t seedLen);
    static void __thiscall AuthChallenge(void* netClient, WowConnection* conn, CDataStore** packet);
};

// ============================================================
//  Helper: parse a WoW 3.3.5a packet header from a raw buffer
//
//  CMSG plaintext layout (before encryption):
//    [2] size BE  (= 4 + payloadLen)
//    [4] opcode LE
//    [N] payload
//
//  SMSG plaintext layout (after decryption):
//    [2] size BE  (= 2 + payloadLen)
//    [2] opcode LE
//    [N] payload
// ============================================================
static bool ParseCMSG(const uint8_t* data, int len, uint16_t& outOpcode, uint32_t& outPayloadLen)
{
    if (len < 6) return false;
    uint16_t sizeField = static_cast<uint16_t>((data[0] << 8) | data[1]);
    outOpcode          = static_cast<uint16_t>( data[2]       | (data[3] << 8));
    outPayloadLen      = static_cast<uint32_t>(sizeField)   - 4;
    return true;
}

static bool ParseSMSG(const uint8_t* data, int len, uint16_t& outOpcode, uint32_t& outPayloadLen)
{
    if (len < 4) return false;
    uint16_t sizeField = static_cast<uint16_t>((data[0] << 8) | data[1]);
    outOpcode          = static_cast<uint16_t>( data[2]       | (data[3] << 8));
    outPayloadLen      = static_cast<uint32_t>(sizeField)   - 2;
    return true;
}

// ============================================================
//  Layer A: WowConnection::Send detour
//
//  This is the ideal CMSG capture point. The CDataStore is in plaintext
//  before any ARC4 header encryption is applied.
//
//  CDataStore layout at call time:
//    m_buffer[0..3]          = opcode (uint32 LE — only low 2 bytes are significant)
//    m_buffer[4..m_size-1]   = payload
//    m_size                  = 4 + payloadLen
// ============================================================
int __thiscall Detours::WowConn_Send(WowConnection* self, CDataStore* packet, int priority)
{
    static int s_first = 1;
    if (s_first) { DebugLog_Log("[PacketHooks] WowConn_Send first call self=%p packet=%p", (void*)self, (void*)packet); s_first = 0; }

    // Register original Send + connection so Replay can inject packets
    PacketReplay::SetSendFn(orig_WowConn_Send, self);

    // Safely peek packet buffer (handshake can pass transient buffers; avoid AV in our code).
    bool safeCapture = false;
    uint16_t opcode = 0;
    uint32_t payloadLen = 0;
    const uint8_t* payloadPtr = nullptr;

    if (packet && packet->m_buffer && packet->m_size >= 4)
    {
        __try
        {
            if (IsReadable(packet->m_buffer, static_cast<size_t>(packet->m_size)))
            {
                opcode     = *reinterpret_cast<const uint16_t*>(packet->m_buffer);
                payloadLen = packet->m_size - 4;
                payloadPtr = (payloadLen > 0) ? (packet->m_buffer + 4) : nullptr;
                safeCapture = true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            safeCapture = false;
        }
    }

    if (safeCapture && PacketCapture::ShouldCapture(PacketDirection::CMSG, opcode))
        PacketCapture::Push(PacketDirection::CMSG, opcode, payloadPtr, payloadLen);

    return orig_WowConn_Send(self, packet, priority);
}

// ============================================================
//  Layer B: ARC4_Process detour  (SMSG only — CMSG handled by Layer A)
//
//  RE signature: SARC4State* __cdecl ARC4_Process(data, len, srcState, dstState)
//  ARC4 only encrypts/decrypts the packet HEADER (SMSG: 4 bytes).
//  Direction: recv when srcState or dstState is &s_activeConn->m_recvCrypto.
// ============================================================
static SARC4State* __cdecl Detour_ARC4_Process(uint8_t* data, uint32_t len, SARC4State* srcState, SARC4State* dstState)
{
    static int s_first = 1;
    if (s_first) { DebugLog_Log("[PacketHooks] ARC4_Process first call data=%p len=%u src=%p dst=%p", (void*)data, (unsigned)len, (void*)srcState, (void*)dstState); s_first = 0; }

    bool isRecv = false;

    if (s_activeConn)
    {
        if (!IsReadable(s_activeConn, sizeof(WowConnection)))
        {
            s_activeConn = nullptr;
            PacketHooks::SetActiveConnection(nullptr);
        }
        else if (len > 0 && (srcState == &s_activeConn->m_recvCrypto || dstState == &s_activeConn->m_recvCrypto))
            isRecv = true;
    }

    SARC4State* result = orig_ARC4_Process(data, len, srcState, dstState);

    // RECV: capture AFTER header decryption.
    if (isRecv && len >= 4)
    {
        uint16_t opcode     = 0;
        uint32_t payloadLen = 0;
        if (ParseSMSG(data, static_cast<int>(len), opcode, payloadLen)
            && PacketCapture::ShouldCapture(PacketDirection::SMSG, opcode))
        {
            const uint8_t* payloadPtr = (payloadLen > 0) ? (data + 4) : nullptr;
            PacketCapture::Push(PacketDirection::SMSG, opcode, payloadPtr, payloadLen);
        }
    }

    return result;
}

// ============================================================
//  Layer C: SetEncryptionKey detour — snapshot keys at login
// ============================================================
void __thiscall Detours::SetEncKey(
    WowConnection* self,
    const uint8_t* sessionKey, uint8_t sessionKeyLen,
    uint8_t serverMode,
    const uint8_t* seed, uint8_t seedLen)
{
    static int s_first = 1;
    if (s_first) { DebugLog_Log("[PacketHooks] SetEncKey first call self=%p", (void*)self); s_first = 0; }

    // Record the connection for ARC4 direction tracking
    s_activeConn = self;
    PacketHooks::SetActiveConnection(self);

    // Snapshot the session key for display in the UI
    if (sessionKey && sessionKeyLen <= 40)
    {
        memcpy(s_sessionKey, sessionKey, sessionKeyLen);
        s_sessionKeyLen = sessionKeyLen;
    }

    // Forward to original
    orig_SetEncKey(self, sessionKey, sessionKeyLen, serverMode, seed, seedLen);

    // After the call, WowConnection has the derived send/recv keys populated.
    // We could snapshot m_sendKey / m_recvKey here for display.
}

// ============================================================
//  Layer D: AuthChallengeHandler detour — log auth event
// ============================================================
void __thiscall Detours::AuthChallenge(void* netClient, WowConnection* conn, CDataStore** packet)
{
    static int s_first = 1;
    if (s_first) { DebugLog_Log("[PacketHooks] AuthChallenge first call netClient=%p conn=%p", (void*)netClient, (void*)conn); s_first = 0; }

    // Record this connection too (may differ from the world conn)
    s_activeConn = conn;
    PacketHooks::SetActiveConnection(conn);

    orig_AuthChallenge(netClient, conn, packet);
}

// ============================================================
//  Public API
// ============================================================
namespace PacketHooks
{
    bool Install()
    {
        DebugLog_Log("[PacketHooks] Install: start");
        bool ok = true;

        // Layer A — WowConnection::Send (CMSG capture: opcode + full payload)
        ok &= HookManager::Add(
            Offsets::WowConn_Send,
            reinterpret_cast<void*>(&Detours::WowConn_Send),
            reinterpret_cast<void**>(&orig_WowConn_Send),
            "WowConn_Send");

        // Layer B — ARC4_Process (SMSG capture after header decryption)
        ok &= HookManager::Add(
            Offsets::ARC4_Process,
            reinterpret_cast<void*>(&Detour_ARC4_Process),
            reinterpret_cast<void**>(&orig_ARC4_Process),
            "ARC4_Process");

        // Layer C — SetEncryptionKey (snapshots session keys, sets s_activeConn)
        ok &= HookManager::Add(
            Offsets::WowConn_SetEncKey,
            reinterpret_cast<void*>(&Detours::SetEncKey),
            reinterpret_cast<void**>(&orig_SetEncKey),
            "WowConn_SetEncKey");

#if !PACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK
        // Layer D — AuthChallengeHandler (login handshake timing + conn tracking)
        ok &= HookManager::Add(
            Offsets::NetClient_AuthChallenge,
            reinterpret_cast<void*>(&Detours::AuthChallenge),
            reinterpret_cast<void**>(&orig_AuthChallenge),
            "NetClient_AuthChallenge");
#endif

        DebugLog_Log("[PacketHooks] Install: %s", ok ? "OK" : "FAIL");
        return ok;
    }

    void Remove()
    {
        HookManager::RemoveAll();
    }

    void SetActiveConnection(WowConnection* conn) { s_activeConn = conn; }
    WowConnection* GetActiveConnection()          { return s_activeConn; }
}
