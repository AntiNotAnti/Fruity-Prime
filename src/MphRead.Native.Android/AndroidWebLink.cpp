#include "AndroidWebLink.hpp"

#if !defined(__ANDROID__)
#error "AndroidWebLink is only valid for the Android native target."
#endif

#include "MainActivity.hpp"

#include "../MphRead.Native/NativeRuntime/System/Console.hpp"
#include "../MphRead.Native/NativeRuntime/System/Encoding.hpp"
#include "../MphRead.Native/NativeRuntime/System/ExceptionText.hpp"

#include <cstdint>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    constexpr std::int32_t FlagActivityNewTask = 0x10000000;

    class ScopedJniEnv final
    {
    public:
        explicit ScopedJniEnv(JavaVM* javaVm)
            : _javaVm(javaVm)
        {
            if (_javaVm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }

            const jint result = _javaVm->GetEnv(
                reinterpret_cast<void**>(&_env),
                JNI_VERSION_1_6);
            if (result == JNI_EDETACHED)
            {
                if (_javaVm->AttachCurrentThread(&_env, nullptr) != JNI_OK)
                {
                    throw std::runtime_error(
                        "could not attach the current thread to the Android Java VM");
                }
                _attached = true;
            }
            else if (result != JNI_OK || _env == nullptr)
            {
                throw std::runtime_error(
                    "could not obtain the Android JNI environment");
            }
        }

        ~ScopedJniEnv()
        {
            if (_attached)
            {
                _javaVm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept
        {
            return _env;
        }

    private:
        JavaVM* _javaVm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    template <typename T>
    class LocalRef final
    {
    public:
        LocalRef(JNIEnv* env, T value) noexcept
            : _env(env), _value(value)
        {
        }

        ~LocalRef()
        {
            if (_env != nullptr && _value != nullptr)
            {
                _env->DeleteLocalRef(_value);
            }
        }

        LocalRef(const LocalRef&) = delete;
        LocalRef& operator=(const LocalRef&) = delete;

        [[nodiscard]] T Get() const noexcept
        {
            return _value;
        }

        explicit operator bool() const noexcept
        {
            return _value != nullptr;
        }

    private:
        JNIEnv* _env = nullptr;
        T _value = nullptr;
    };

    [[nodiscard]] std::string JavaExceptionMessage(
        JNIEnv* env,
        jthrowable throwable)
    {
        if (throwable == nullptr)
        {
            return "Android Java exception";
        }

        LocalRef<jclass> throwableClass(env, env->GetObjectClass(throwable));
        if (env->ExceptionCheck() || !throwableClass)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        const jmethodID getMessage = env->GetMethodID(
            throwableClass.Get(),
            "getMessage",
            "()Ljava/lang/String;");
        if (env->ExceptionCheck() || getMessage == nullptr)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        LocalRef<jstring> message(
            env,
            static_cast<jstring>(env->CallObjectMethod(throwable, getMessage)));
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        if (!message)
        {
            return "Android Java exception";
        }

        const jsize length = env->GetStringLength(message.Get());
        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        const jchar* chars = env->GetStringChars(message.Get(), nullptr);
        if (env->ExceptionCheck() || chars == nullptr)
        {
            env->ExceptionClear();
            return "Android Java exception";
        }
        std::string result;
        try
        {
            result = MphRead::NativeRuntime::Utf16ToUtf8(
                std::u16string_view(
                    reinterpret_cast<const char16_t*>(chars),
                    static_cast<std::size_t>(length)));
        }
        catch (...)
        {
            env->ReleaseStringChars(message.Get(), chars);
            throw;
        }
        env->ReleaseStringChars(message.Get(), chars);
        return result.empty() ? "Android Java exception" : result;
    }

    [[noreturn]] void ThrowPendingJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> throwable(
            env,
            static_cast<jthrowable>(env->ExceptionOccurred()));
        env->ExceptionClear();
        throw std::runtime_error(JavaExceptionMessage(env, throwable.Get()));
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowPendingJavaException(env);
        }
    }

    [[nodiscard]] jstring NewJavaString(JNIEnv* env, std::string_view value)
    {
        const std::u16string text = MphRead::NativeRuntime::Utf8ToUtf16(value);
        if (text.size() > static_cast<std::size_t>(
                std::numeric_limits<jsize>::max()))
        {
            throw std::length_error("string is too long for Android JNI");
        }
        static_assert(sizeof(char16_t) == sizeof(jchar));
        jstring result = env->NewString(
            reinterpret_cast<const jchar*>(text.data()),
            static_cast<jsize>(text.size()));
        CheckJavaException(env);
        if (result == nullptr)
        {
            throw std::bad_alloc();
        }
        return result;
    }

    [[nodiscard]] jmethodID GetMethodId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature)
    {
        const jmethodID method = env->GetMethodID(type, name, signature);
        CheckJavaException(env);
        if (method == nullptr)
        {
            throw std::runtime_error(std::string("Android method not found: ") + name);
        }
        return method;
    }

    [[nodiscard]] jmethodID GetStaticMethodId(
        JNIEnv* env,
        jclass type,
        const char* name,
        const char* signature)
    {
        const jmethodID method = env->GetStaticMethodID(type, name, signature);
        CheckJavaException(env);
        if (method == nullptr)
        {
            throw std::runtime_error(std::string("Android method not found: ") + name);
        }
        return method;
    }

    [[nodiscard]] jclass FindClass(JNIEnv* env, const char* name)
    {
        jclass type = env->FindClass(name);
        CheckJavaException(env);
        if (type == nullptr)
        {
            throw std::runtime_error(std::string("Android class not found: ") + name);
        }
        return type;
    }

    void DeleteGlobalRefNoThrow(JavaVM* javaVm, jobject value) noexcept
    {
        if (javaVm == nullptr || value == nullptr)
        {
            return;
        }
        JNIEnv* env = nullptr;
        bool attached = false;
        const jint result = javaVm->GetEnv(
            reinterpret_cast<void**>(&env),
            JNI_VERSION_1_6);
        if (result == JNI_EDETACHED)
        {
            if (javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            {
                return;
            }
            attached = true;
        }
        else if (result != JNI_OK || env == nullptr)
        {
            return;
        }
        env->DeleteGlobalRef(value);
        if (attached)
        {
            javaVm->DetachCurrentThread();
        }
    }
}

namespace MphRead::Droid
{
    AndroidWebLink::AndroidWebLink(JNIEnv* env, jobject context)
    {
        if (env == nullptr)
        {
            throw std::invalid_argument("Android JNI environment must not be null");
        }
        if (env->GetJavaVM(&_javaVm) != JNI_OK || _javaVm == nullptr)
        {
            throw std::runtime_error("Android Java VM is not available");
        }
        if (context != nullptr)
        {
            _context = env->NewGlobalRef(context);
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                throw std::runtime_error(
                    "Android Java exception while retaining the application Context");
            }
            if (_context == nullptr)
            {
                throw std::bad_alloc();
            }
        }
    }

    AndroidWebLink::~AndroidWebLink()
    {
        DeleteGlobalRefNoThrow(_javaVm, _context);
    }

    bool AndroidWebLink::Open(std::string_view url)
    {
        try
        {
            ScopedJniEnv scopedEnv(_javaVm);
            JNIEnv* env = scopedEnv.Get();
            jobject context = _context;
            if (MainActivity* activity = MainActivity::Instance();
                activity != nullptr)
            {
                context = activity->_activity;
            }
            if (context == nullptr)
            {
                throw std::runtime_error(
                    "Object reference not set to an instance of an object.");
            }

            LocalRef<jclass> uriClass(env, FindClass(env, "android/net/Uri"));
            const jmethodID parse = GetStaticMethodId(
                env,
                uriClass.Get(),
                "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;");
            LocalRef<jstring> javaUrl(env, NewJavaString(env, url));
            LocalRef<jobject> uri(
                env,
                env->CallStaticObjectMethod(uriClass.Get(), parse, javaUrl.Get()));
            CheckJavaException(env);

            LocalRef<jclass> intentClass(
                env,
                FindClass(env, "android/content/Intent"));
            const jmethodID intentConstructor = GetMethodId(
                env,
                intentClass.Get(),
                "<init>",
                "(Ljava/lang/String;Landroid/net/Uri;)V");
            LocalRef<jstring> action(
                env,
                NewJavaString(env, "android.intent.action.VIEW"));
            LocalRef<jobject> intent(
                env,
                env->NewObject(
                    intentClass.Get(),
                    intentConstructor,
                    action.Get(),
                    uri.Get()));
            CheckJavaException(env);

            LocalRef<jclass> activityClass(
                env,
                FindClass(env, "android/app/Activity"));
            const bool isActivity = env->IsInstanceOf(context, activityClass.Get());
            CheckJavaException(env);
            if (!isActivity)
            {
                const jmethodID addFlags = GetMethodId(
                    env,
                    intentClass.Get(),
                    "addFlags",
                    "(I)Landroid/content/Intent;");
                (void)env->CallObjectMethod(
                    intent.Get(), addFlags, FlagActivityNewTask);
                CheckJavaException(env);
            }

            LocalRef<jclass> contextClass(env, env->GetObjectClass(context));
            CheckJavaException(env);
            if (!contextClass)
            {
                throw std::runtime_error("Android Context class could not be obtained");
            }
            const jmethodID startActivity = GetMethodId(
                env,
                contextClass.Get(),
                "startActivity",
                "(Landroid/content/Intent;)V");
            env->CallVoidMethod(context, startActivity, intent.Get());
            CheckJavaException(env);
            return true;
        }
        catch (...)
        {
            const std::exception_ptr error = std::current_exception();
            MphRead::NativeRuntime::ConsoleWriteLine(
                "[android] could not open " + std::string(url) + ": "
                    + MphRead::NativeRuntime::ExceptionMessage(error));
            return false;
        }
    }
}
