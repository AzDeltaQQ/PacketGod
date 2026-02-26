#pragma once
#include <cstdint>

// ============================================================
//  PacketGod — WoW 3.3.5a Build 12340 — Known Addresses
//  All offsets relative to the wow.exe module base (0x00400000)
//  unless marked as absolute VA (verified against IDA).
// ============================================================

namespace Offsets
{
    // --------------------------------------------------------
    //  Cryptography
    // --------------------------------------------------------
    constexpr uintptr_t ARC4_Init             = 0x00775040;  // void ARC4_Init(SARC4State*, const uint8_t* key, int keyLen)
    constexpr uintptr_t ARC4_Process          = 0x00774EA0;  // SARC4State* __cdecl ARC4_Process(data, len, srcState, dstState)

    constexpr uintptr_t SHA1_Prepare          = 0x0077AAA0;  // SHA1::Init
    constexpr uintptr_t SHA1_Process          = 0x006CA180;  // SHA1::Update
    constexpr uintptr_t SHA1_Finish           = 0x006CA270;  // SHA1::Final

    // --------------------------------------------------------
    //  WowConnection
    // --------------------------------------------------------
    constexpr uintptr_t WowConn_Send          = 0x004675F0;  // ::Send(this, CDataStore*, priority) — Layer A hook target
    constexpr uintptr_t WowConn_Encrypt       = 0x004665B0;  // ::Encrypt — XORs 6-byte CMSG header with ARC4
    constexpr uintptr_t WowConn_SetStatus     = 0x004667C0;  // ::SetStatus(this, int) — e.g. 7=error/closing
    constexpr uintptr_t WowConn_SetEncKey     = 0x00466BF0;  // ::SetEncryptionKey(this, sessionKey, keyLen, serverMode, seed, seedLen)
    constexpr uintptr_t WowConn_SetEncryption = 0x00466820;  // ::SetEncryption – activates ARC4 on the connection
    constexpr uintptr_t WowConn_Disconnect    = 0x00466B50;  // ::Disconnect
    constexpr uintptr_t WowConn_HMAC_Prepare  = 0x004668A0;  // internal HMAC context init

    // WowConnection Init — check here to verify full object size
    constexpr uintptr_t WowConn_Init          = 0x004669D0;  // TODO: verify

    // --------------------------------------------------------
    //  NetClient
    // --------------------------------------------------------
    constexpr uintptr_t NetClient_AuthChallenge = 0x00632730; // SMSG_AUTH_CHALLENGE handler
    constexpr uintptr_t NetClient_SolvePoW      = 0x006321B0; // SHA1 proof-of-work solver
    constexpr uintptr_t NetClient_AddEvent      = 0x00633650; // NETEVENTQUEUE::AddEvent
    constexpr uintptr_t NetClient_Send          = 0x00632B50; // prepends opcode to CDataStore then calls WowConn_Send

    // --------------------------------------------------------
    //  CDataStore — key serialization helpers
    // --------------------------------------------------------
    constexpr uintptr_t CDataStore_Constructor  = 0x00401050; // CDataStore::CDataStore()
    constexpr uintptr_t CDataStore_Finalize     = 0x00401130; // resets m_read to 0 (write→read mode)
    constexpr uintptr_t CDataStore_Reset        = 0x004010E0; // clears buffer, m_read = -1
    constexpr uintptr_t CDataStore_Put_uint8    = 0x0047AFE0;
    constexpr uintptr_t CDataStore_Put_uint16   = 0x0047B040;
    constexpr uintptr_t CDataStore_Put_uint32   = 0x0047B0A0;
    constexpr uintptr_t CDataStore_Put_uint64   = 0x0047B100;
    constexpr uintptr_t CDataStore_PutArray     = 0x0047B1C0; // raw byte array
    constexpr uintptr_t CDataStore_PutString    = 0x0047B300; // null-terminated string
    constexpr uintptr_t CDataStore_Get_uint8    = 0x0047B340;
    constexpr uintptr_t CDataStore_Get_uint16   = 0x0047B380;
    constexpr uintptr_t CDataStore_Get_uint32   = 0x0047B3C0;
    constexpr uintptr_t CDataStore_GetDataInSitu = 0x0047B6B0; // zero-copy read, returns ptr into buffer

    // --------------------------------------------------------
    //  Globals
    // --------------------------------------------------------
    constexpr uintptr_t g_DefaultSeed   = 0x009E8A9C;  // 04 AE 98 CC … (static encryption seed)
    constexpr uintptr_t g_Drop1024Buf   = 0x00B39160;  // 1024-byte zero buffer used for ARC4 Drop1024
    constexpr uintptr_t g_CDataStore_vt = 0x009E0E24;  // CDataStore vtable

    // --------------------------------------------------------
    //  TODO — Locate via further RE
    // --------------------------------------------------------
    // NetClient::HandlePacket  = ???   incoming SMSG dispatch table
}
