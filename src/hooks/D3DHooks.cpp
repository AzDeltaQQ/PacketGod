#include "D3DHooks.h"
#include "HookManager.h"
#include "../ui/PacketUI.h"
#include "../DebugLog.h"

#include <Windows.h>
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

// ImGui headers — include order matters
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

// ============================================================
//  D3D9 vtable slot indices
// ============================================================
static constexpr int kPresent_Slot  = 17;
static constexpr int kEndScene_Slot = 42;
static constexpr int kReset_Slot    = 16;

// ============================================================
//  State
// ============================================================
using fn_Present  = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using fn_Reset    = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

static fn_Present  orig_Present  = nullptr;
static fn_Reset    orig_Reset    = nullptr;

static bool s_imguiReady  = false;
static bool s_showUI      = true;
static HWND s_gameWnd     = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============================================================
//  WndProc subclass — route input to ImGui when UI is open
// ============================================================
static WNDPROC s_origWndProc = nullptr;

static bool IsMouseMessage(UINT msg)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

static bool IsKeyboardMessage(UINT msg)
{
    switch (msg)
    {
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR:
    case WM_IME_CHAR: case WM_IME_KEYDOWN: case WM_IME_KEYUP:
        return true;
    default:
        return false;
    }
}

static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_showUI && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 0;

    // When ImGui is capturing input (e.g. dragging window, scrolling in list), don't pass to game
    if (s_showUI && s_imguiReady)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && IsMouseMessage(msg))
            return 0;
        if (io.WantCaptureKeyboard && IsKeyboardMessage(msg))
            return 0;
    }

    // Toggle UI with Insert key
    if (msg == WM_KEYDOWN && wParam == VK_INSERT)
    {
        s_showUI = !s_showUI;
        return 0;
    }

    return CallWindowProcW(s_origWndProc, hWnd, msg, wParam, lParam);
}

// ============================================================
//  Present hook — render ImGui every frame
// ============================================================
static HRESULT __stdcall Detour_Present(
    IDirect3DDevice9* device,
    const RECT* pSourceRect,
    const RECT* pDestRect,
    HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion)
{
    if (!s_imguiReady)
    {
        s_gameWnd = FindWindowW(L"GxWindowClass", nullptr);
        if (!s_gameWnd && hDestWindowOverride && IsWindow(hDestWindowOverride))
            s_gameWnd = hDestWindowOverride;
        // WoW sometimes calls Present with hDestOverride=NULL. Fallback: main window of this process.
        if (!s_gameWnd)
        {
            struct { DWORD pid; HWND out; } ctx = { GetCurrentProcessId(), nullptr };
            EnumWindows([](HWND h, LPARAM lp) -> BOOL {
                auto* c = reinterpret_cast<decltype(ctx)*>(lp);
                DWORD wpid = 0;
                GetWindowThreadProcessId(h, &wpid);
                if (wpid == c->pid && IsWindowVisible(h)) { c->out = h; return FALSE; }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&ctx));
            if (ctx.out)
                s_gameWnd = ctx.out;
        }
        DebugLog_Log("[D3D] Present: chosen wnd=%p", (void*)s_gameWnd);

        if (s_gameWnd)
        {
            DebugLog_Log("[D3D] Present: init ImGui, wnd=%p", (void*)s_gameWnd);
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();

            ImGui_ImplWin32_Init(s_gameWnd);
            ImGui_ImplDX9_Init(device);

            s_origWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(s_gameWnd, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(HookedWndProc)));

            s_imguiReady = true;
            DebugLog_Log("[D3D] Present: ImGui ready");
        }
        else
            DebugLog_Log("[D3D] Present: no valid window, skipping ImGui");
    }

    if (s_imguiReady && (!s_gameWnd || !IsWindow(s_gameWnd)))
    {
        DebugLog_Log("[D3D] Present: window stale, re-attach");
        HWND newWnd = FindWindowW(L"GxWindowClass", nullptr);
        if (!newWnd && hDestWindowOverride && IsWindow(hDestWindowOverride))
            newWnd = hDestWindowOverride;
        if (!newWnd)
        {
            DWORD pid = GetCurrentProcessId();
            for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT))
            {
                DWORD wpid = 0;
                GetWindowThreadProcessId(h, &wpid);
                if (wpid == pid && IsWindowVisible(h)) { newWnd = h; break; }
            }
        }
        if (newWnd && newWnd != s_gameWnd)
        {
            s_gameWnd = newWnd;
            ImGui_ImplWin32_Shutdown();
            ImGui_ImplWin32_Init(s_gameWnd);
            s_origWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(s_gameWnd, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(HookedWndProc)));
            DebugLog_Log("[D3D] Present: re-attached to %p", (void*)s_gameWnd);
        }
    }

    if (!s_imguiReady)
    {
        static int s_skipCount = 0;
        if (s_skipCount < 5 || (s_skipCount % 60) == 0)
            DebugLog_Log("[D3D] Present: bypass (no ImGui) frame %d", s_skipCount);
        s_skipCount++;
        return orig_Present(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (s_showUI)
        PacketUI::Render();
    else
    {
        // Hint when UI is hidden so user can bring it back with INSERT
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::Begin("PacketGod", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("Press INSERT to open PacketGod");
        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    DebugLog_Flush();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    HRESULT hr = orig_Present(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    return hr;
}

// ============================================================
//  Reset hook — release ImGui D3D9 resources before device reset
// ============================================================
static HRESULT __stdcall Detour_Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pParams)
{
    if (s_imguiReady)
        ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT hr = orig_Reset(device, pParams);

    if (SUCCEEDED(hr) && s_imguiReady)
        ImGui_ImplDX9_CreateDeviceObjects();

    return hr;
}

// ============================================================
//  Locate the D3D9 device vtable by creating a dummy device.
//  We keep the dummy device and D3D9 alive so the vtable we hook
//  is never freed (releasing them caused ACCESS_VIOLATION when
//  the game called Present through the same runtime vtable).
// ============================================================
static IDirect3D9*         s_dummyD3D  = nullptr;
static IDirect3DDevice9*   s_dummyDev  = nullptr;
static HWND                s_dummyWnd  = nullptr;

static bool GetD3D9VTable(void**& outVtable)
{
    DebugLog_Log("[D3D] GetD3D9VTable: start");
    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"PacketGod_D3D9Dummy";
    RegisterClassExW(&wc);

    s_dummyWnd = CreateWindowExW(0, L"PacketGod_D3D9Dummy", L"", WS_OVERLAPPED,
                                 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (!s_dummyWnd) return false;

    s_dummyD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!s_dummyD3D) { DestroyWindow(s_dummyWnd); s_dummyWnd = nullptr; return false; }

    D3DPRESENT_PARAMETERS pp = {};
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = s_dummyWnd;
    pp.Windowed         = TRUE;

    HRESULT hr = s_dummyD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, s_dummyWnd,
                                          D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &s_dummyDev);
    if (FAILED(hr))
    {
        pp.BackBufferWidth  = 1;
        pp.BackBufferHeight = 1;
        hr = s_dummyD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, s_dummyWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &s_dummyDev);
    }

    if (FAILED(hr) || !s_dummyDev)
    {
        if (s_dummyD3D) { s_dummyD3D->Release(); s_dummyD3D = nullptr; }
        if (s_dummyWnd) { DestroyWindow(s_dummyWnd); s_dummyWnd = nullptr; }
        UnregisterClassW(L"PacketGod_D3D9Dummy", wc.hInstance);
        return false;
    }

    outVtable = *reinterpret_cast<void***>(s_dummyDev);
    DebugLog_Log("[D3D] GetD3D9VTable: OK vtable=%p (dummy device kept alive)", (void*)outVtable);
    return true;
}

// ============================================================
//  Public API
// ============================================================
namespace D3DHooks
{
    bool Install()
    {
        DebugLog_Log("[D3D] Install: start");
        void** vtable = nullptr;
        if (!GetD3D9VTable(vtable))
        {
            DebugLog_Log("[D3D] Install: GetD3D9VTable failed");
            return false;
        }

        bool ok = true;
        ok &= HookManager::Add(
            reinterpret_cast<uintptr_t>(vtable[kPresent_Slot]),
            reinterpret_cast<void*>(&Detour_Present),
            reinterpret_cast<void**>(&orig_Present),
            "IDirect3DDevice9::Present");

        ok &= HookManager::Add(
            reinterpret_cast<uintptr_t>(vtable[kReset_Slot]),
            reinterpret_cast<void*>(&Detour_Reset),
            reinterpret_cast<void**>(&orig_Reset),
            "IDirect3DDevice9::Reset");

        DebugLog_Log("[D3D] Install: %s", ok ? "OK" : "FAIL");
        return ok;
    }

    void Remove()
    {
        if (s_imguiReady && s_gameWnd && s_origWndProc)
            SetWindowLongPtrW(s_gameWnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(s_origWndProc));

        if (s_imguiReady)
        {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            s_imguiReady = false;
        }

        if (s_dummyDev) { s_dummyDev->Release(); s_dummyDev = nullptr; }
        if (s_dummyD3D) { s_dummyD3D->Release(); s_dummyD3D = nullptr; }
        if (s_dummyWnd)
        {
            DestroyWindow(s_dummyWnd);
            s_dummyWnd = nullptr;
            UnregisterClassW(L"PacketGod_D3D9Dummy", GetModuleHandleW(nullptr));
        }
    }

    bool IsImGuiReady() { return s_imguiReady; }
}
