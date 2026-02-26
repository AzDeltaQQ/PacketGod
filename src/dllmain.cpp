#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <thread>

#include "DebugLog.h"
#include "hooks/HookManager.h"
#include "hooks/PacketHooks.h"
#include "hooks/D3DHooks.h"
#include "packet/PacketCapture.h"

// ============================================================
//  PacketGod — WoW 3.3.5a (build 12340) packet tool
//  Injected as a DLL into wow.exe.
//
//  Boot sequence:
//    1. DLL_PROCESS_ATTACH fires on a new thread
//    2. MinHook is initialized
//    3. D3D9 hooks installed (ImGui rendering)
//    4. Packet hooks installed (ARC4, SetEncryptionKey, AuthChallenge)
//    5. All hooks enabled
//
//  Shutdown (DLL_PROCESS_DETACH or eject hotkey):
//    6. Hooks disabled + removed
//    7. ImGui torn down
//    8. MinHook uninitialized
// ============================================================

static HMODULE s_hSelf = nullptr;

// ============================================================
//  Worker thread — runs while DLL is loaded
// ============================================================
static DWORD WINAPI WorkerThread(LPVOID)
{
    DebugLog_Init(s_hSelf);
    DebugLog_Log("[PacketGod] WorkerThread started");

    Sleep(500);
    DebugLog_Log("[PacketGod] After Sleep(500)");

    DebugLog_Log("[PacketGod] HookManager::Init ...");
    if (!HookManager::Init())
    {
        DebugLog_Log("[PacketGod] HookManager::Init FAILED");
        MessageBoxW(nullptr, L"HookManager::Init failed", L"PacketGod", MB_ICONERROR);
        return 1;
    }
    DebugLog_Log("[PacketGod] HookManager::Init OK");

    DebugLog_Log("[PacketGod] D3DHooks::Install ...");
    if (!D3DHooks::Install())
    {
        DebugLog_Log("[PacketGod] D3DHooks::Install failed (ImGui disabled)");
    }
    else
        DebugLog_Log("[PacketGod] D3DHooks::Install OK");

    DebugLog_Log("[PacketGod] PacketHooks::Install ...");
    if (!PacketHooks::Install())
        DebugLog_Log("[PacketGod] PacketHooks::Install failed");
    else
        DebugLog_Log("[PacketGod] PacketHooks::Install OK");

    DebugLog_Log("[PacketGod] HookManager::EnableAll ...");
    HookManager::EnableAll();
    DebugLog_Log("[PacketGod] EnableAll done - Loaded. Press INSERT to toggle UI.");

    while (true)
    {
        Sleep(100);
        if (GetAsyncKeyState(VK_END) & 1)
            break;
    }

    DebugLog_Log("[PacketGod] Ejecting...");
    HookManager::DisableAll();
    PacketHooks::Remove();
    D3DHooks::Remove();
    HookManager::Shutdown();
    DebugLog_Shutdown();
    FreeLibraryAndExitThread(s_hSelf, 0);
}

// ============================================================
//  DLL entry point
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        s_hSelf = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }
    return TRUE;
}
