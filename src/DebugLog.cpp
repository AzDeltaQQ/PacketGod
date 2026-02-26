#include "DebugLog.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

static FILE* s_logFile = nullptr;
static HANDLE s_mutex = nullptr;
static bool s_inited = false;

static void LogLock()
{
    if (s_mutex)
        WaitForSingleObject(s_mutex, INFINITE);
}

static void LogUnlock()
{
    if (s_mutex)
        ReleaseMutex(s_mutex);
}

void DebugLog_Init(HMODULE hModule)
{
    if (s_inited) return;

    s_mutex = CreateMutexW(nullptr, FALSE, nullptr);
    if (!s_mutex) return;

    wchar_t path[MAX_PATH] = {};
    if (hModule && GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
    {
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (lastSlash)
        {
            lastSlash[1] = L'\0';
            wcscat_s(path, L"debuglog.txt");
            _wfopen_s(&s_logFile, path, L"w");
        }
    }

    if (!s_logFile)
    {
        s_logFile = fopen("debuglog.txt", "w");
    }

    s_inited = true;

    LogLock();
    if (s_logFile)
    {
        fprintf(s_logFile, "========== PacketGod session ==========\n");
        fprintf(s_logFile, "Log file: debuglog.txt\n");
        fflush(s_logFile);
    }
    LogUnlock();
}

void DebugLog_Log(const char* fmt, ...)
{
    if (!s_inited || !s_logFile) return;

    LogLock();
    if (s_logFile)
    {
        va_list args;
        va_start(args, fmt);
        vfprintf(s_logFile, fmt, args);
        va_end(args);
        fputc('\n', s_logFile);
        fflush(s_logFile);
    }
    LogUnlock();
}

void DebugLog_Flush()
{
    LogLock();
    if (s_logFile)
        fflush(s_logFile);
    LogUnlock();
}

void DebugLog_Shutdown()
{
    LogLock();
    if (s_logFile)
    {
        fclose(s_logFile);
        s_logFile = nullptr;
    }
    s_inited = false;
    LogUnlock();
    if (s_mutex)
    {
        CloseHandle(s_mutex);
        s_mutex = nullptr;
    }
}
