#include "PacketCapture.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>

// ============================================================

static uint64_t s_qpcFreq = 0;

uint64_t PacketCapture::NowMicros()
{
    if (!s_qpcFreq)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        s_qpcFreq = static_cast<uint64_t>(freq.QuadPart);
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!s_startTime)
    {
        s_startTime = static_cast<uint64_t>(now.QuadPart);
        return 0;
    }
    uint64_t delta = static_cast<uint64_t>(now.QuadPart) - s_startTime;
    return delta * 1'000'000ULL / s_qpcFreq;
}

// ============================================================
//  Filter helpers
// ============================================================

bool PacketCapture::ShouldCapture(PacketDirection dir, uint16_t opcode)
{
    std::lock_guard<std::mutex> lk(s_mutex);
    for (const auto& f : s_filters)
    {
        if (!f.enabled) continue;
        bool dirMatch = f.matchAny || (f.direction == dir);
        bool opcodeMatch = (f.opcode == 0) || (f.opcode == opcode);
        if (dirMatch && opcodeMatch)
        {
            if (f.blockPacket)
            {
                ++s_totalDropped;
                return false;
            }
        }
    }
    return true;
}

// ============================================================
//  Push  (called from game thread)
// ============================================================

void PacketCapture::Push(PacketDirection dir, uint16_t opcode,
                         const uint8_t* payload, uint32_t size)
{
    CapturedPacket pkt;
    pkt.direction    = dir;
    pkt.opcode       = opcode;
    pkt.size         = size;
    pkt.timestamp_us = NowMicros();
    if (payload && size > 0)
        pkt.payload.assign(payload, payload + size);

    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_ring.size() >= kMaxHistory)
        s_ring.pop_front();
    s_ring.push_back(std::move(pkt));
    ++s_totalCaptured;
}

// ============================================================
//  Snapshot  (called from render thread)
// ============================================================

std::vector<CapturedPacket> PacketCapture::Snapshot()
{
    std::lock_guard<std::mutex> lk(s_mutex);
    return { s_ring.begin(), s_ring.end() };
}

void PacketCapture::Clear()
{
    std::lock_guard<std::mutex> lk(s_mutex);
    s_ring.clear();
    s_totalCaptured = 0;
    s_totalDropped  = 0;
}

// ============================================================
//  Filter management
// ============================================================

void PacketCapture::AddFilter(const FilterRule& rule)
{
    std::lock_guard<std::mutex> lk(s_mutex);
    s_filters.push_back(rule);
}

void PacketCapture::RemoveFilter(size_t index)
{
    std::lock_guard<std::mutex> lk(s_mutex);
    if (index < s_filters.size())
        s_filters.erase(s_filters.begin() + index);
}

void PacketCapture::ClearFilters()
{
    std::lock_guard<std::mutex> lk(s_mutex);
    s_filters.clear();
}

const std::vector<FilterRule>& PacketCapture::GetFilters()
{
    // Caller must hold their own lock if needed; safe for single-threaded UI read
    return s_filters;
}

uint64_t PacketCapture::TotalCaptured() { return s_totalCaptured; }
uint64_t PacketCapture::TotalDropped()  { return s_totalDropped;  }
