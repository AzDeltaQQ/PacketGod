#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <MinHook.h>

// ============================================================
//  HookManager — thin RAII wrapper around MinHook
//  Usage:
//    HookManager::Init();
//    HookManager::Add(targetVA, detourFn, &originalFn);
//    HookManager::EnableAll();
//    ...
//    HookManager::RemoveAll();  // call before FreeLibrary
// ============================================================

class HookManager
{
public:
    static bool Init();
    static void Shutdown();

    // Register a hook.  outOriginal receives the trampoline pointer.
    static bool Add(uintptr_t targetVA, void* detour, void** outOriginal, const char* debugName = nullptr);

    static bool EnableAll();
    static bool DisableAll();
    static void RemoveAll();

private:
    struct HookEntry
    {
        void* target;
        std::string name;
    };
    static inline std::vector<HookEntry> s_hooks;
    static inline bool                   s_initialized = false;
};
