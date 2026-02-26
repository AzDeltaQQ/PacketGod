#pragma once

#include <Windows.h>

// Initialize debug log. Call once from DllMain or WorkerThread with the DLL's HMODULE.
// Log file: debuglog.txt in the same folder as PacketGod.dll.
void DebugLog_Init(HMODULE hModule);

// Log a line (printf-style). Thread-safe; flushes after each write.
void DebugLog_Log(const char* fmt, ...);

// Flush log file.
void DebugLog_Flush();

// Close log file (call on shutdown if desired).
void DebugLog_Shutdown();
