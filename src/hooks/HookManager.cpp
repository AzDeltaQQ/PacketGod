#include "HookManager.h"
#include "../DebugLog.h"
#include <stdexcept>

bool HookManager::Init()
{
    if (s_initialized) { DebugLog_Log("[HookManager] Init (already inited)"); return true; }
    DebugLog_Log("[HookManager] MH_Initialize ...");
    MH_STATUS st = MH_Initialize();
    DebugLog_Log("[HookManager] MH_Initialize => %d", (int)st);
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    s_initialized = true;
    return true;
}

void HookManager::Shutdown()
{
    DebugLog_Log("[HookManager] Shutdown");
    DisableAll();
    MH_Uninitialize();
    s_hooks.clear();
    s_initialized = false;
}

bool HookManager::Add(uintptr_t targetVA, void* detour, void** outOriginal, const char* debugName)
{
    void* target = reinterpret_cast<void*>(targetVA);
    MH_STATUS st = MH_CreateHook(target, detour, outOriginal);
    DebugLog_Log("[HookManager] Add %s va=0x%08X => %d", debugName ? debugName : "(null)", (unsigned)targetVA, (int)st);
    if (st != MH_OK) return false;
    s_hooks.push_back({ target, debugName ? debugName : "" });
    return true;
}

bool HookManager::EnableAll()
{
    DebugLog_Log("[HookManager] EnableAll ...");
    bool ok = MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
    DebugLog_Log("[HookManager] EnableAll => %s", ok ? "OK" : "FAIL");
    return ok;
}

bool HookManager::DisableAll()
{
    DebugLog_Log("[HookManager] DisableAll");
    return MH_DisableHook(MH_ALL_HOOKS) == MH_OK;
}

void HookManager::RemoveAll()
{
    DebugLog_Log("[HookManager] RemoveAll (%zu hooks)", s_hooks.size());
    for (auto& h : s_hooks)
        MH_RemoveHook(h.target);
    s_hooks.clear();
}
