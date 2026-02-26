#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <string>
#include <functional>
#include "../wow/WowTypes.h"

// ============================================================
//  PacketCapture — thread-safe ring buffer for captured packets
//
//  Hooks call PacketCapture::Push() from the game thread.
//  The ImGui UI reads via PacketCapture::Snapshot() on the
//  render thread.  A mutex protects the shared deque.
// ============================================================

struct FilterRule
{
    bool        enabled      = false;
    uint16_t    opcode       = 0;            // 0 = match any
    PacketDirection direction = PacketDirection::CMSG;
    bool        matchAny     = true;         // true = wildcard direction
    bool        blockPacket  = false;        // true = drop instead of log
};

class PacketCapture
{
public:
    static constexpr size_t kMaxHistory = 2048;

    // Called by hooks —————————————————————————————————————————
    static void Push(PacketDirection dir, uint16_t opcode,
                     const uint8_t* payload, uint32_t size);

    // UI accessors ————————————————————————————————————————————
    // Returns a stable snapshot (copy) for the UI thread.
    static std::vector<CapturedPacket> Snapshot();

    static void Clear();

    // Filter management ———————————————————————————————————————
    static void         AddFilter(const FilterRule& rule);
    static void         RemoveFilter(size_t index);
    static void         ClearFilters();
    static const std::vector<FilterRule>& GetFilters();

    // Returns false if a "block" rule matches (hook should drop the packet)
    static bool ShouldCapture(PacketDirection dir, uint16_t opcode);

    // Stats ———————————————————————————————————————————————————
    static uint64_t TotalCaptured();
    static uint64_t TotalDropped();

    // Microsecond timestamp relative to DLL load
    static uint64_t NowMicros();

private:
    static inline std::mutex                s_mutex;
    static inline std::deque<CapturedPacket> s_ring;
    static inline std::vector<FilterRule>    s_filters;
    static inline uint64_t                   s_totalCaptured = 0;
    static inline uint64_t                   s_totalDropped  = 0;
    static inline uint64_t                   s_startTime     = 0;  // QueryPerformanceCounter epoch
};
