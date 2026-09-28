#pragma once

#if !defined(__ANDROID__)
#error "AndroidWebLink is only valid for the Android native target."
#endif

#include "../MphRead.Native/Mods/Platform/WebLink.hpp"

#include <jni.h>

namespace MphRead::Droid
{
    class AndroidWebLink final : public MphRead::Mods::Platform::IWebLink
    {
    public:
        AndroidWebLink(JNIEnv* env, jobject context);
        ~AndroidWebLink() override;

        AndroidWebLink(const AndroidWebLink&) = delete;
        AndroidWebLink& operator=(const AndroidWebLink&) = delete;
        AndroidWebLink(AndroidWebLink&&) = delete;
        AndroidWebLink& operator=(AndroidWebLink&&) = delete;

        [[nodiscard]] bool Open(std::string_view url) override;

    private:
        JavaVM* _javaVm = nullptr;
        jobject _context = nullptr;
    };
}
