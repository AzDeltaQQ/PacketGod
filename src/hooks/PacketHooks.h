#pragma once
#include <cstdint>
#include "../wow/WowTypes.h"

// ============================================================
//  PacketHooks — MinHook detours for packet interception
//
//  Layer A: WowConnection::Send (0x004675F0)  ← IMPLEMENTED
//    → Best for CMSG. CDataStore has opcode + full payload
//      in plaintext before ARC4 header encryption is applied.
//      layout: m_buffer[0..3]=opcode(LE), m_buffer[4..m_size-1]=payload.
//
//  Layer B: ARC4_Process (0x00774EA0)  ← IMPLEMENTED (SMSG only)
//    → Only encrypts/decrypts the 4-byte SMSG header (or 6-byte CMSG
//      header). On recv, data+4 points to the plaintext payload in the
//      same recv buffer. CMSG capture removed — Layer A is used instead.
//
//  Layer C: WowConnection::SetEncryptionKey (0x00466BF0)  ← IMPLEMENTED
//    → Fired once at login. Snapshots session key + sets s_activeConn
//      so Layer B can identify send vs recv SARC4State by pointer comparison.
//
//  Layer D: NetClient::AuthChallengeHandler (0x00632730)  ← IMPLEMENTED
//    → Fires during login handshake. Secondary conn tracking / timing.
//    → If login crashes at 0x00467C0B (AV read 0x436DECA2), try building with
//      -DPACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK=1 to test.
// ============================================================

namespace PacketHooks
{
    // Install all hooks (call after HookManager::Init)
    bool Install();

    // Remove all hooks (call before FreeLibrary / shutdown)
    void Remove();

    // Set the active WowConnection* so ARC4 hook can identify direction.
    // Call this from the SetEncryptionKey hook.
    void SetActiveConnection(WowConnection* conn);
    WowConnection* GetActiveConnection();
}
