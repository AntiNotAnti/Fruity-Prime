#pragma once

#include <string>

namespace MphRead::NativeRuntime
{
    // System.Runtime.InteropServices.NativeLibrary, the operations used by the
    // compatibility check to load a file and verify one of its exports.
    [[nodiscard]] void* NativeLibraryLoad(const std::string& path);
    [[nodiscard]] void* NativeLibraryGetExport(void* handle, const std::string& name);
    void NativeLibraryFree(void* handle) noexcept;
}
