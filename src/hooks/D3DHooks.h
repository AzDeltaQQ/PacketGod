#pragma once

// ============================================================
//  D3DHooks — Hook IDirect3DDevice9::Present to render ImGui
//
//  WoW 3.3.5a uses Direct3D 9.  We find the vtable by creating
//  a throw-away device on a hidden HWND, reading vtable slot 17
//  (Present), and hooking it with MinHook.
//
//  EndScene (slot 42) is an alternative hook point — it fires
//  once per frame before Present and is sometimes more stable.
// ============================================================

namespace D3DHooks
{
    bool Install();
    void Remove();
    bool IsImGuiReady();
}
