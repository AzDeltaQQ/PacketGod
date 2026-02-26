#pragma once
#include <cstdint>
#include "Offsets.h"
#include "WowTypes.h"

// ============================================================
//  Typed function pointers for calling game functions directly.
//  Casting through these avoids raw reinterpret_cast at call sites.
// ============================================================

// ---- Crypto ------------------------------------------------

// void ARC4_Init(SARC4State* state, const uint8_t* key, int keyLen)
using fn_ARC4_Init    = void(__cdecl*)(SARC4State*, const uint8_t*, int);

// void ARC4_Process(SARC4State* state, uint8_t* data, int len)
using fn_ARC4_Process = void(__cdecl*)(SARC4State*, uint8_t*, int);

// void SHA1_Prepare(SHA1_CONTEXT*)
using fn_SHA1_Prepare = void(__cdecl*)(SHA1_CONTEXT*);

// void SHA1_Process(SHA1_CONTEXT*, const uint8_t* data, uint32_t len)
using fn_SHA1_Process = void(__cdecl*)(SHA1_CONTEXT*, const uint8_t*, uint32_t);

// void SHA1_Finish(SHA1_CONTEXT*, uint8_t digest[20])
using fn_SHA1_Finish  = void(__cdecl*)(SHA1_CONTEXT*, uint8_t*);

// ---- WowConnection -----------------------------------------

// int __thiscall WowConnection::Send(this, CDataStore* packet, int priority)
// Called with plaintext CDataStore before header encryption — our Layer A hook target.
using fn_WowConn_Send =
    int(__thiscall*)(WowConnection*, CDataStore*, int);

// void __thiscall WowConnection::SetEncryptionKey(this, sessionKey, keyLen, serverMode, seed, seedLen)
using fn_WowConn_SetEncKey =
    void(__thiscall*)(WowConnection*, const uint8_t*, uint8_t, uint8_t, const uint8_t*, uint8_t);

// void __thiscall WowConnection::Disconnect(this)
using fn_WowConn_Disconnect = void(__thiscall*)(WowConnection*);

// ---- Convenient inline callers -----------------------------
namespace Game
{
    inline void ARC4_Init(SARC4State* s, const uint8_t* key, int len)
    {
        reinterpret_cast<fn_ARC4_Init>(Offsets::ARC4_Init)(s, key, len);
    }

    inline void ARC4_Process(SARC4State* s, uint8_t* data, int len)
    {
        reinterpret_cast<fn_ARC4_Process>(Offsets::ARC4_Process)(s, data, len);
    }

    inline void SHA1_Init(SHA1_CONTEXT* ctx)
    {
        reinterpret_cast<fn_SHA1_Prepare>(Offsets::SHA1_Prepare)(ctx);
    }

    inline void SHA1_Update(SHA1_CONTEXT* ctx, const uint8_t* data, uint32_t len)
    {
        reinterpret_cast<fn_SHA1_Process>(Offsets::SHA1_Process)(ctx, data, len);
    }

    inline void SHA1_Final(SHA1_CONTEXT* ctx, uint8_t* digest)
    {
        reinterpret_cast<fn_SHA1_Finish>(Offsets::SHA1_Finish)(ctx, digest);
    }

    // Send a packet to the server. `packet` must be in read mode (Finalize called).
    // Returns 0=sent, 1=queued, 2=error.
    inline int WowConn_Send(WowConnection* conn, CDataStore* packet, int priority = 0)
    {
        return reinterpret_cast<fn_WowConn_Send>(Offsets::WowConn_Send)(conn, packet, priority);
    }
}
