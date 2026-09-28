#pragma once

// Temporary diagnostics for the freeze investigation: one line a second of
// what the GPU side is doing, written straight to disk so a hard reset does
// not take the last lines with it. Remove once the freeze is found.

#include <atomic>
#include <cstdint>

namespace MphRead::NativeRuntime::GpuTrace
{
    // Texture uploads the UI surface made, and how many bytes they carried.
    inline std::atomic<std::int64_t> Uploads{0};
    inline std::atomic<std::int64_t> UploadBytes{0};

    // Called around every buffer swap by the window.
    void BeforeSwap();
    void AfterSwap();
}
