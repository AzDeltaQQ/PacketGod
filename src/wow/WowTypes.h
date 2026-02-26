#pragma once
#include <cstdint>
#include <vector>

// ============================================================
//  PacketGod — WoW 3.3.5a Build 12340 — Reverse-Engineered Types
// ============================================================

// ------------------------------------------------------------
//  ARC4 stream cipher state  (256-byte S-box + i/j indices)
// ------------------------------------------------------------
struct SARC4State
{
    /* +0x000 */ uint8_t  m_sbox[256];
    /* +0x100 */ uint8_t  m_i;
    /* +0x101 */ uint8_t  m_j;
    // No padding — game struct is exactly 258 (0x102) bytes.
    // WowConnection::m_recvCrypto = m_sendCrypto + 0x102 = +0x24A (verified via SetEncryptionKey RE).
};
static_assert(sizeof(SARC4State) == 0x102, "SARC4State size mismatch");

// ------------------------------------------------------------
//  SHA1 context  (standard 92-byte structure)
// ------------------------------------------------------------
struct SHA1_CONTEXT
{
    /* +0x000 */ uint32_t state[5];
    /* +0x014 */ uint32_t count[2];
    /* +0x01C */ uint8_t  buffer[64];
};
static_assert(sizeof(SHA1_CONTEXT) == 92, "SHA1_CONTEXT size mismatch");

// ------------------------------------------------------------
//  CDataStore  (packet buffer / reader-writer)
// ------------------------------------------------------------
struct CDataStore
{
    /* +0x000 */ void*    m_vtable;     // @ 0x009E0E24
    /* +0x004 */ uint8_t* m_buffer;
    /* +0x008 */ uint32_t m_base;
    /* +0x00C */ uint32_t m_alloc;
    /* +0x010 */ uint32_t m_size;
    /* +0x014 */ uint32_t m_readPos;
};

// ------------------------------------------------------------
//  WowConnection  (one TCP connection to realm/world server)
//  Offsets verified via WowConnection::Send + SetEncryptionKey RE.
// ------------------------------------------------------------
struct WowConnection
{
    /* +0x000 */ void*      m_vtable;
    /* +0x004 */ int        m_socket;       // WinSock SOCKET handle

    /* +0x008 */ uint32_t   m_refCount;
    /* +0x00C */ uint8_t    _pad0[4];
    /* +0x010 */ int32_t    m_status;       // 5=Connected, 7=Closing/Error
    /* +0x014 */ void*      m_handler;      // WowConnectionHandler*

    /* +0x018 */ uint8_t    _pad1[200];     // unanalyzed members up to send queue

    /* +0x0E0 */ uint8_t    _sendQueue[12]; // TSList (send queue head/tail/lock)
    /* +0x0EC */ int32_t    m_sendDepth;    // current # of queued packets
    /* +0x0F0 */ int32_t    m_sendBytes;    // total bytes in queue
    /* +0x0F4 */ int32_t    m_maxSendDepth; // queue depth limit (SErrDisplayAppFatal if exceeded)

    /* +0x0F8 */ uint8_t    _pad2[16];

    /* +0x108 */ uint8_t    _sendCrit[32];  // SCritSect (CRITICAL_SECTION wrapper)
    /* +0x128 */ uint8_t    _pad3[32];

    /* +0x148 */ SARC4State m_sendCrypto;   // ARC4 state for outgoing bytes  (+0x148)
    /* +0x24A */ SARC4State m_recvCrypto;   // ARC4 state for incoming bytes  (+0x24A = +0x148 + 0x102)

    /* +0x34C */ uint8_t    m_sendKey[20];  // derived HMAC-SHA1 send key
    /* +0x360 */ uint8_t    m_recvKey[20];  // derived HMAC-SHA1 recv key

    /* +0x374 */ uint8_t    m_isEncrypted;  // non-zero = ARC4 header encryption active
    /* +0x375 */ uint8_t    m_headerLenSend;
    /* +0x376 */ uint8_t    m_headerLenRecv;
    /* +0x377 */ uint8_t    _pad4[1];
};

// ------------------------------------------------------------
//  NetClient  (top-level world network manager)
// ------------------------------------------------------------
struct NetClient
{
    /* +0x000 */ void*          m_vtable;
    /* +0x004 */ char           m_accountName[1284];  // null-terminated
    /* +0x508 */ uint8_t        m_sessionKey[40];     // SRP6 S (40 bytes)
    /* +0x530 */ uint8_t        _pad[9476];

    /* +0x2E34 */ void*         m_eventQueue;          // NETEVENTQUEUE*
    /* +0x2E38 */ WowConnection* m_realmConnection;    // login/auth socket
    /* +0x2E3C */ WowConnection* m_clientConnection;   // world socket
};

// ------------------------------------------------------------
//  Captured packet record (our own, not game-engine)
// ------------------------------------------------------------
enum class PacketDirection : uint8_t
{
    CMSG = 0,  // Client → Server
    SMSG = 1,  // Server → Client
};

struct CapturedPacket
{
    PacketDirection direction;
    uint16_t        opcode;
    uint32_t        size;          // payload size (bytes after opcode)
    uint64_t        timestamp_us;  // microseconds since DLL load
    std::vector<uint8_t> payload;  // raw payload bytes (opcode stripped)
};
