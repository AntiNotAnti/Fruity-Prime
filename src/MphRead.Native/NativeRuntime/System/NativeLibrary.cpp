#include "NativeLibrary.hpp"

#include "Encoding.hpp"
#include "Exceptions.hpp"

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace MphRead::NativeRuntime
{
    void* NativeLibraryLoad(const std::string& path)
    {
#if defined(_WIN32)
        HMODULE handle = ::LoadLibraryW(Wtf8ToWide(path).c_str());
        if (handle == nullptr)
        {
            throw System::DllNotFoundException("Unable to load native library '" + path
                + "' (error " + std::to_string(::GetLastError()) + ").");
        }
        return reinterpret_cast<void*>(handle);
#else
        static_cast<void>(::dlerror());
        void* const handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr)
        {
            const char* const error = ::dlerror();
            throw System::DllNotFoundException("Unable to load native library '" + path
                + "': " + (error == nullptr ? "unknown loader error" : error));
        }
        return handle;
#endif
    }

    void* NativeLibraryGetExport(void* handle, const std::string& name)
    {
#if defined(_WIN32)
        const FARPROC address = ::GetProcAddress(reinterpret_cast<HMODULE>(handle), name.c_str());
        if (address == nullptr)
        {
            throw System::EntryPointNotFoundException("The entry point '" + name
                + "' was not found in the native library.");
        }
        return reinterpret_cast<void*>(address);
#else
        static_cast<void>(::dlerror());
        void* const address = ::dlsym(handle, name.c_str());
        const char* const error = ::dlerror();
        if (error != nullptr)
        {
            throw System::EntryPointNotFoundException("The entry point '" + name
                + "' was not found in the native library: " + error);
        }
        return address;
#endif
    }

    void NativeLibraryFree(void* handle) noexcept
    {
        if (handle == nullptr)
        {
            return;
        }
#if defined(_WIN32)
        static_cast<void>(::FreeLibrary(reinterpret_cast<HMODULE>(handle)));
#else
        static_cast<void>(::dlclose(handle));
#endif
    }
}
