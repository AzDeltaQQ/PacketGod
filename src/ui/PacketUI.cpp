#include "PacketUI.h"
#include "../packet/PacketCapture.h"
#include "../packet/PacketReplay.h"
#include "../hooks/PacketHooks.h"
#include "../wow/WowTypes.h"

#include <Windows.h>

#include <imgui.h>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>

// Opcodes.h is at src/Opcodes.h (CMAKE includes src and project root)
#include "Opcodes.h"

// ============================================================
//  Helpers
// ============================================================

static const char* DirectionStr(PacketDirection d)
{
    return d == PacketDirection::CMSG ? "CMSG" : "SMSG";
}

// Render a 16-column hex+ASCII dump of arbitrary bytes
static void HexDump(const uint8_t* data, uint32_t size)
{
    constexpr int kCols = 16;
    char line[128];
    for (uint32_t row = 0; row < size; row += kCols)
    {
        // Offset
        int pos = snprintf(line, sizeof(line), "%04X  ", row);

        // Hex bytes
        for (int col = 0; col < kCols; ++col)
        {
            uint32_t idx = row + col;
            if (idx < size)
                pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[idx]);
            else
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            if (col == 7) pos += snprintf(line + pos, sizeof(line) - pos, " ");
        }

        pos += snprintf(line + pos, sizeof(line) - pos, " |");

        // ASCII
        for (int col = 0; col < kCols; ++col)
        {
            uint32_t idx = row + col;
            if (idx >= size) break;
            uint8_t c = data[idx];
            pos += snprintf(line + pos, sizeof(line) - pos, "%c",
                            (c >= 0x20 && c < 0x7F) ? (char)c : '.');
        }
        pos += snprintf(line + pos, sizeof(line) - pos, "|");
        ImGui::TextUnformatted(line);
    }
}

// ============================================================
//  Module-level UI state
// ============================================================

static int  s_selected     = -1;          // selected row in packet list
static bool s_autoScroll   = true;
static char s_filterText[64] = {};        // opcode name/number filter
static bool s_showCMSG     = true;
static bool s_showSMSG     = true;

// Replay / edit state
static std::vector<CapturedPacket> s_editBuffer;  // packets staged for replay
static char s_editHex[4096] = {};                  // hex editor text
static char s_replayDelayMs[8] = "0";

// Filter rule builder
static char s_ruleOpcode[8]   = {};
static bool s_ruleBlock       = false;
static bool s_ruleCMSG        = true;
static bool s_ruleSMSG        = true;

// Snapshot updated once per frame
static std::vector<CapturedPacket> s_snapshot;

// ============================================================
//  Sub-windows
// ============================================================

// Height budget constants (fractions of the available content region)
static constexpr float kListFraction   = 0.48f;  // packet list
static constexpr float kDetailFraction = 0.20f;  // hex detail panel
// remaining fraction goes to the tab bar area

static void DrawPacketList(float height)
{
    ImGui::BeginChild("##PacketList", ImVec2(0, height), false);

    // Column headers
    ImGui::Columns(5, "pkt_cols", true);
    ImGui::SetColumnWidth(0, 75);   // Time
    ImGui::SetColumnWidth(1, 58);   // Dir
    ImGui::SetColumnWidth(2, 72);   // Opcode
    ImGui::SetColumnWidth(3, 210);  // Name
    ImGui::SetColumnWidth(4, 62);   // Size

    ImGui::TextDisabled("Time(ms)"); ImGui::NextColumn();
    ImGui::TextDisabled("Dir");      ImGui::NextColumn();
    ImGui::TextDisabled("Opcode");   ImGui::NextColumn();
    ImGui::TextDisabled("Name");     ImGui::NextColumn();
    ImGui::TextDisabled("Size");     ImGui::NextColumn();
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(s_snapshot.size()); ++i)
    {
        const auto& pkt = s_snapshot[i];

        // Direction filter
        if (!s_showCMSG && pkt.direction == PacketDirection::CMSG) continue;
        if (!s_showSMSG && pkt.direction == PacketDirection::SMSG) continue;

        // Text filter (opcode hex, decimal, or name substring match)
        if (s_filterText[0] != '\0')
        {
            char opcodeStr[16];
            snprintf(opcodeStr, sizeof(opcodeStr), "%04X", pkt.opcode);
            char opcodeDecStr[16];
            snprintf(opcodeDecStr, sizeof(opcodeDecStr), "%u", pkt.opcode);
            const char* name = OpcodeToString(pkt.opcode);
            if (!strstr(opcodeStr, s_filterText) &&
                !strstr(opcodeDecStr, s_filterText) &&
                !strstr(name, s_filterText))
                continue;
        }

        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%.3f", pkt.timestamp_us / 1000.0);

        char opcodeStr[10];
        snprintf(opcodeStr, sizeof(opcodeStr), "0x%04X", pkt.opcode);

        bool isCMSG = pkt.direction == PacketDirection::CMSG;
        ImVec4 color = isCMSG ? ImVec4(0.4f, 0.8f, 1.0f, 1.0f)
                               : ImVec4(0.55f, 1.0f, 0.55f, 1.0f);

        bool isSelected = (s_selected == i);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        ImGui::Text("%s", timeStr); ImGui::NextColumn();

        if (ImGui::Selectable(DirectionStr(pkt.direction),
                               isSelected,
                               ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0)))
        {
            s_selected = i;
            std::string hexStr;
            for (uint8_t b : pkt.payload)
            {
                char h[4];
                snprintf(h, sizeof(h), "%02X ", b);
                hexStr += h;
            }
            size_t copyLen = (std::min)(hexStr.size(), sizeof(s_editHex) - 1);
            strncpy_s(s_editHex, sizeof(s_editHex), hexStr.c_str(), copyLen);
        }
        ImGui::NextColumn();

        ImGui::Text("%s", opcodeStr); ImGui::NextColumn();
        ImGui::Text("%s", OpcodeToString(static_cast<Opcodes>(pkt.opcode))); ImGui::NextColumn();
        ImGui::Text("%u", pkt.size);  ImGui::NextColumn();

        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::Columns(1);

    if (s_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

static void DrawDetailPanel(float height)
{
    if (s_selected < 0 || s_selected >= static_cast<int>(s_snapshot.size()))
    {
        ImGui::BeginChild("##DetailEmpty", ImVec2(0, height), true);
        ImGui::TextDisabled("Select a packet to inspect.");
        ImGui::EndChild();
        return;
    }

    const CapturedPacket& pkt = s_snapshot[s_selected];

    // One-line summary bar
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                       "0x%04X  %s  %u bytes  %.3f ms",
                       pkt.opcode,
                       DirectionStr(pkt.direction),
                       pkt.size,
                       pkt.timestamp_us / 1000.0);

    // Hex dump fills the remaining height
    float dumpHeight = height - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##HexDump", ImVec2(0, dumpHeight), true);
    if (!pkt.payload.empty())
        HexDump(pkt.payload.data(), static_cast<uint32_t>(pkt.payload.size()));
    else
        ImGui::TextDisabled("(empty payload)");
    ImGui::EndChild();
}

// ============================================================
//  Replay / Editor tab
// ============================================================
static void DrawReplayTab(float availHeight)
{
    const float lineH     = ImGui::GetFrameHeightWithSpacing();
    const float spacing   = ImGui::GetStyle().ItemSpacing.y;

    // Reserve space for: buttons row + sep + delay row + replay btn + popup headroom
    const float fixedRows = lineH * 4.0f + spacing * 4.0f;
    const float remaining = availHeight - fixedRows;
    // Split remaining 40% staged list / 60% hex editor
    const float stagedH   = (std::max)(remaining * 0.38f, 48.0f);
    const float editorH   = (std::max)(remaining * 0.55f, 48.0f);

    // Buttons
    if (s_selected >= 0 && s_selected < static_cast<int>(s_snapshot.size()))
    {
        const CapturedPacket& pkt = s_snapshot[s_selected];
        if (ImGui::Button("Stage Selected"))
            s_editBuffer.push_back(pkt);
        ImGui::SameLine();
    }
    if (ImGui::Button("Clear Staged"))
        s_editBuffer.clear();

    ImGui::Separator();

    ImGui::TextDisabled("Staged packets");
    ImGui::BeginChild("##Staged", ImVec2(0, stagedH), true);
    for (int i = 0; i < static_cast<int>(s_editBuffer.size()); ++i)
    {
        const auto& p = s_editBuffer[i];
        char label[64];
        snprintf(label, sizeof(label), "[%d] %s 0x%04X (%u bytes)##staged%d",
                 i, DirectionStr(p.direction), p.opcode, p.size, i);
        if (ImGui::Selectable(label, false))
        {
            std::string hexStr;
            for (uint8_t b : p.payload)
            {
                char h[4];
                snprintf(h, sizeof(h), "%02X ", b);
                hexStr += h;
            }
            size_t copyLen = (std::min)(hexStr.size(), sizeof(s_editHex) - 1);
            strncpy_s(s_editHex, sizeof(s_editHex), hexStr.c_str(), copyLen);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("Edit payload (hex bytes, space-separated):");
    ImGui::InputTextMultiline("##hexeditor", s_editHex, sizeof(s_editHex),
                               ImVec2(-1, editorH));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Delay (ms):"); ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::InputText("##delay", s_replayDelayMs, sizeof(s_replayDelayMs),
                     ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    if (ImGui::Button("Replay All Staged"))
    {
        if (PacketReplay::IsReady())
        {
            uint32_t delay = static_cast<uint32_t>(atoi(s_replayDelayMs));
            PacketReplay::ReplaySequence(s_editBuffer, delay);
        }
        else
        {
            ImGui::OpenPopup("replay_notready");
        }
    }

    if (ImGui::BeginPopupModal("replay_notready", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("WowConnection::Send not yet hooked.\nConnect to world server first.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ============================================================
//  Filter / Rules tab
// ============================================================
static void DrawFiltersTab(float /*availHeight*/)
{
    ImGui::Text("Add filter rule:");
    ImGui::SetNextItemWidth(80);
    ImGui::InputText("Opcode (hex, 0=any)##rule", s_ruleOpcode, sizeof(s_ruleOpcode),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::Checkbox("CMSG##rule", &s_ruleCMSG);
    ImGui::SameLine();
    ImGui::Checkbox("SMSG##rule", &s_ruleSMSG);
    ImGui::SameLine();
    ImGui::Checkbox("Block##rule", &s_ruleBlock);
    ImGui::SameLine();

    if (ImGui::Button("Add Rule"))
    {
        FilterRule r;
        r.enabled     = true;
        r.opcode      = static_cast<uint16_t>(strtol(s_ruleOpcode, nullptr, 16));
        r.blockPacket = s_ruleBlock;
        r.matchAny    = (s_ruleCMSG && s_ruleSMSG);
        r.direction   = s_ruleCMSG ? PacketDirection::CMSG : PacketDirection::SMSG;
        PacketCapture::AddFilter(r);
    }

    ImGui::Separator();

    const auto& filters = PacketCapture::GetFilters();
    ImGui::Text("%zu active rules:", filters.size());
    for (size_t i = 0; i < filters.size(); ++i)
    {
        const auto& f = filters[i];
        char label[128];
        snprintf(label, sizeof(label), "[%zu] opcode=0x%04X dir=%s block=%s##f%zu",
                 i,
                 f.opcode,
                 f.matchAny ? "ANY" : DirectionStr(f.direction),
                 f.blockPacket ? "YES" : "no",
                 i);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        char btn[32];
        snprintf(btn, sizeof(btn), "X##del%zu", i);
        if (ImGui::SmallButton(btn))
            PacketCapture::RemoveFilter(i);
    }
}

// ============================================================
//  Stats tab
// ============================================================
static void DrawStatsTab(float /*availHeight*/)
{
    WowConnection* conn = PacketHooks::GetActiveConnection();

    ImGui::Text("Packets captured : %llu", PacketCapture::TotalCaptured());
    ImGui::Text("Packets dropped  : %llu", PacketCapture::TotalDropped());
    ImGui::Text("Replay ready     : %s",   PacketReplay::IsReady() ? "YES" : "no");
    ImGui::Separator();
    ImGui::Text("WowConnection*   : %p", conn);

    // Only dereference conn if it's still readable (avoids AV if connection was closed/freed).
    if (conn && IsBadReadPtr(conn, sizeof(WowConnection)) == 0)
    {
        ImGui::Text("Encrypted        : %u", static_cast<unsigned>(conn->m_isEncrypted));
        ImGui::Text("Header send/recv : %u / %u",
                    conn->m_headerLenSend, conn->m_headerLenRecv);

        if (ImGui::TreeNode("Send Key (20 bytes)"))
        {
            HexDump(conn->m_sendKey, 20);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Recv Key (20 bytes)"))
        {
            HexDump(conn->m_recvKey, 20);
            ImGui::TreePop();
        }
    }
    else if (conn)
        ImGui::TextColored(ImVec4(1,0.4f,0,1), "(connection pointer stale)");
}

// ============================================================
//  Main render entry
// ============================================================
void PacketUI::Render()
{
    s_snapshot = PacketCapture::Snapshot();

    ImGui::SetNextWindowSize(ImVec2(820, 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20),    ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));

    if (!ImGui::Begin("PacketGod  [INSERT to toggle]",
                      nullptr,
                      ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }

    // ── Toolbar ────────────────────────────────────────────────
    ImGui::Checkbox("CMSG", &s_showCMSG); ImGui::SameLine();
    ImGui::Checkbox("SMSG", &s_showSMSG); ImGui::SameLine();
    ImGui::TextDisabled("|"); ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("Filter", s_filterText, sizeof(s_filterText));
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &s_autoScroll);
    ImGui::SameLine();
    if (ImGui::Button("Clear Log"))
    {
        PacketCapture::Clear();
        s_selected = -1;
    }
    ImGui::Separator();

    // ── Proportional height budget ─────────────────────────────
    // Measure what's left below the toolbar so panels scale with the window.
    const float totalAvail = ImGui::GetContentRegionAvail().y;

    // Leave a small fixed gap between panels (2 separators + spacing)
    const float gapH       = ImGui::GetStyle().ItemSpacing.y * 2.0f + 2.0f;
    const float tabBarH    = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;

    const float listH      = totalAvail * kListFraction;
    const float detailH    = totalAvail * kDetailFraction;
    const float tabBodyH   = totalAvail - listH - detailH - tabBarH - gapH * 3.0f;

    // ── Packet list ────────────────────────────────────────────
    DrawPacketList(listH);

    ImGui::Separator();

    // ── Detail / hex dump ──────────────────────────────────────
    DrawDetailPanel(detailH);

    ImGui::Separator();

    // ── Tab bar ────────────────────────────────────────────────
    if (ImGui::BeginTabBar("##tabs"))
    {
        if (ImGui::BeginTabItem("Replay / Edit"))
        {
            DrawReplayTab(tabBodyH);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Filters"))
        {
            DrawFiltersTab(tabBodyH);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Stats / Keys"))
        {
            DrawStatsTab(tabBodyH);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
