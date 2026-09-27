#include "GpuTrace.hpp"

#include "../System/Runtime.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <io.h>
#include <tlhelp32.h>
#endif

namespace MphRead::NativeRuntime::GpuTrace
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        using GetIntegerv = void(APIENTRY*)(unsigned int, int*);
        using GetError = unsigned int(APIENTRY*)();
        using GetString = const unsigned char*(APIENTRY*)(unsigned int);

        constexpr unsigned int GpuMemoryAvailableNvx = 0x9049;
        constexpr unsigned int TextureFreeMemoryAti = 0x87FC;
        constexpr unsigned int GlVendor = 0x1F00;
        constexpr unsigned int GlRenderer = 0x1F01;

        struct State
        {
            std::FILE* File = nullptr;
            bool Opened = false;
            Clock::time_point WindowStart = Clock::now();
            Clock::time_point LastSwapEnd{};
            Clock::time_point SwapStart{};
            std::int64_t Frames = 0;
            double SwapTotal = 0;
            double SwapMax = 0;
            double GapMax = 0;
            std::int64_t Errors = 0;
            std::int64_t TotalFrames = 0;
            GetIntegerv Integer = nullptr;
            GetError Error = nullptr;
        };

        State& S()
        {
            static State state;
            return state;
        }

        void Open(State& s)
        {
            s.Opened = true;
            try
            {
                const std::filesystem::path dir = std::filesystem::path(AppContextBaseDirectory()) / "logs";
                std::filesystem::create_directories(dir);
                char name[64];
                const std::time_t now = std::time(nullptr);
                std::tm local{};
#if defined(_WIN32)
                localtime_s(&local, &now);
#else
                localtime_r(&now, &local);
#endif
                std::strftime(name, sizeof(name), "gpu-trace-%Y%m%d-%H%M%S.txt", &local);
                s.File = std::fopen((dir / name).string().c_str(), "w");
            }
            catch (...)
            {
                s.File = nullptr;
            }
            s.Integer = reinterpret_cast<GetIntegerv>(::glfwGetProcAddress("glGetIntegerv"));
            s.Error = reinterpret_cast<GetError>(::glfwGetProcAddress("glGetError"));
            if (s.File != nullptr)
            {
                const auto string = reinterpret_cast<GetString>(::glfwGetProcAddress("glGetString"));
                const unsigned char* vendor = string != nullptr ? string(GlVendor) : nullptr;
                const unsigned char* renderer = string != nullptr ? string(GlRenderer) : nullptr;
                std::fprintf(s.File, "gpu trace: %s / %s\n", vendor != nullptr ? reinterpret_cast<const char*>(vendor) : "?",
                    renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "?");
                std::fprintf(s.File, "time frames fps swapAvgMs swapMaxMs gapMaxMs glErrors uploads uploadMB vidFreeMB privMB gdi user threads handles sysCommitPct\n");
                std::fflush(s.File);
            }
        }

        void Flush(std::FILE* file)
        {
            std::fflush(file);
#if defined(_WIN32)
            // Past the C runtime and the OS cache: a hard reset keeps it.
            ::FlushFileBuffers(reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(file))));
#endif
        }
    }

    void BeforeSwap()
    {
        State& s = S();
        if (!s.Opened)
        {
            Open(s);
        }
        s.SwapStart = Clock::now();
        if (s.LastSwapEnd != Clock::time_point{})
        {
            s.GapMax = std::max(s.GapMax,
                std::chrono::duration<double, std::milli>(s.SwapStart - s.LastSwapEnd).count());
        }
        if (s.Error != nullptr)
        {
            // Drain every pending error, capped so a broken context cannot spin.
            for (int i = 0; i < 16 && s.Error() != 0; i++)
            {
                s.Errors++;
            }
        }
    }

    void AfterSwap()
    {
        State& s = S();
        const Clock::time_point now = Clock::now();
        const double swap = std::chrono::duration<double, std::milli>(now - s.SwapStart).count();
        s.SwapTotal += swap;
        s.SwapMax = std::max(s.SwapMax, swap);
        s.LastSwapEnd = now;
        s.Frames++;
        s.TotalFrames++;
        const double window = std::chrono::duration<double>(now - s.WindowStart).count();
        if (window < 1.0 || s.File == nullptr)
        {
            return;
        }
        int vidFree = -1;
        if (s.Integer != nullptr)
        {
            int value = 0;
            s.Integer(GpuMemoryAvailableNvx, &value);
            if (s.Error != nullptr && s.Error() == 0 && value > 0)
            {
                vidFree = value / 1024;
            }
            else
            {
                int ati[4]{};
                s.Integer(TextureFreeMemoryAti, ati);
                if (s.Error != nullptr && s.Error() == 0 && ati[0] > 0)
                {
                    vidFree = ati[0] / 1024;
                }
            }
        }
        long long priv = -1;
        unsigned long gdi = 0;
        unsigned long user = 0;
        unsigned long threads = 0;
        unsigned long handles = 0;
        int commitPct = -1;
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS_EX counters{};
        if (::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)))
        {
            priv = static_cast<long long>(counters.PrivateUsage / (1024 * 1024));
        }
        gdi = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
        user = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
        DWORD handleCount = 0;
        ::GetProcessHandleCount(::GetCurrentProcess(), &handleCount);
        handles = handleCount;
        const HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE)
        {
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            const DWORD self = ::GetCurrentProcessId();
            for (BOOL more = ::Thread32First(snap, &entry); more; more = ::Thread32Next(snap, &entry))
            {
                if (entry.th32OwnerProcessID == self)
                {
                    threads++;
                }
            }
            ::CloseHandle(snap);
        }
        PERFORMANCE_INFORMATION perf{};
        if (::GetPerformanceInfo(&perf, sizeof(perf)) && perf.CommitLimit > 0)
        {
            commitPct = static_cast<int>(perf.CommitTotal * 100 / perf.CommitLimit);
        }
#endif
        const std::time_t wall = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &wall);
#else
        localtime_r(&wall, &local);
#endif
        char stamp[16];
        std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);
        std::fprintf(s.File, "%s %lld %.1f %.2f %.1f %.1f %lld %lld %.1f %d %lld %lu %lu %lu %lu %d\n", stamp,
            static_cast<long long>(s.TotalFrames), s.Frames / window, s.SwapTotal / std::max<std::int64_t>(1, s.Frames),
            s.SwapMax, s.GapMax, static_cast<long long>(s.Errors), static_cast<long long>(Uploads.load()),
            static_cast<double>(UploadBytes.load()) / (1024 * 1024), vidFree, priv, gdi, user, threads, handles, commitPct);
        Flush(s.File);
        s.WindowStart = now;
        s.Frames = 0;
        s.SwapTotal = 0;
        s.SwapMax = 0;
        s.GapMax = 0;
    }
}
