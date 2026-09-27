#include "ThumbnailBatch.hpp"

#include "ThumbnailCapture.hpp"
#include "ThumbnailGenerator.hpp"
#include "ThumbnailLog.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/Exceptions.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#elif defined(__APPLE__)
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#endif
#endif

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::EnvironmentProcessPath;
using ::MphRead::NativeRuntime::PasteArgument;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::Utf8ToUtf16;
using ::MphRead::NativeRuntime::Utf8ToWide;
using ::MphRead::NativeRuntime::WideToUtf8;

namespace
{
    using MphRead::Mods::ThumbnailCapture;
    using MphRead::Mods::ThumbnailGenerator;
    using MphRead::Mods::ThumbnailLog;

    class PendingSet final
    {
    public:
        explicit PendingSet(const std::vector<std::string>& rooms)
        {
            _values.reserve(rooms.size());
            for (const std::string& room : rooms)
            {
                if (!Contains(room))
                {
                    _values.push_back(room);
                }
            }
        }

        [[nodiscard]] bool Contains(const std::string& room) const
        {
            return std::any_of(_values.begin(), _values.end(), [&](const std::string& value)
            {
                return ::MphRead::NativeRuntime::StringCompareOrdinalIgnoreCase(value, room) == 0;
            });
        }

        bool Remove(const std::string& room)
        {
            const auto found = std::find_if(_values.begin(), _values.end(), [&](const std::string& value)
            {
                return ::MphRead::NativeRuntime::StringCompareOrdinalIgnoreCase(value, room) == 0;
            });
            if (found == _values.end())
            {
                return false;
            }
            _values.erase(found);
            return true;
        }

    private:
        std::vector<std::string> _values;
    };

#ifdef _WIN32
    std::string WindowsErrorMessage(DWORD error)
    {
        if (error == ERROR_BAD_EXE_FORMAT
            || error == ERROR_EXE_MACHINE_TYPE_MISMATCH)
        {
            return "The specified executable is not a valid application for this OS platform.";
        }

        LPWSTR buffer = nullptr;
        const DWORD length = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
        if (length == 0 || buffer == nullptr)
        {
            return std::system_category().message(static_cast<int>(error));
        }

        std::wstring_view message(buffer, length);
        while (!message.empty()
            && (message.back() == L'\r' || message.back() == L'\n'))
        {
            message.remove_suffix(1);
        }
        std::string result = WideToUtf8(message);
        ::LocalFree(buffer);
        return result;
    }

    std::mutex WindowsProcessStartMutex;
    std::atomic_uint64_t WindowsPipeSequence{0};

    struct WindowsPipe
    {
        HANDLE Read = nullptr;
        HANDLE Write = nullptr;
    };

    WindowsPipe CreateRedirectPipe()
    {
        const std::uint64_t sequence =
            WindowsPipeSequence.fetch_add(1, std::memory_order_relaxed);
        const std::wstring pipeName =
            L"\\\\.\\pipe\\LOCAL\\dotnet_"
            + std::to_wstring(::GetCurrentProcessId()) + L"_"
            + std::to_wstring(sequence);

        HANDLE read = ::CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
            1,
            0,
            4 * 4096,
            120000,
            nullptr);
        if (read == INVALID_HANDLE_VALUE)
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }

        HANDLE write = ::CreateFileW(
            pipeName.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
            nullptr);
        if (write == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(read);
            throw std::system_error(static_cast<int>(error), std::system_category());
        }
        return {read, write};
    }

    HANDLE DuplicateAsInheritable(HANDLE source, bool& duplicate)
    {
        duplicate = false;
        if (source == nullptr || source == INVALID_HANDLE_VALUE)
        {
            return source;
        }

        DWORD flags = 0;
        if (::GetHandleInformation(source, &flags)
            && (flags & HANDLE_FLAG_INHERIT) != 0)
        {
            return source;
        }

        HANDLE inherited = nullptr;
        if (!::DuplicateHandle(
            ::GetCurrentProcess(), source,
            ::GetCurrentProcess(), &inherited,
            0, TRUE, DUPLICATE_SAME_ACCESS))
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }
        duplicate = true;
        return inherited;
    }
#endif

    std::runtime_error ProcessStartFailure(
        const std::string& exePath,
        const std::filesystem::path& workingDirectory,
        const std::string& errorMessage)
    {
        return std::runtime_error(
            "An error occurred trying to start process '" + exePath
            + "' with working directory '" + PathToUtf8(workingDirectory)
            + "'. " + errorMessage);
    }

    class WorkerProcess;
    std::mutex ProcessLock;
    std::vector<std::shared_ptr<WorkerProcess>> ActiveWorkers;
    bool Exiting = false;
    bool ExitHandlerRegistered = false;
    std::recursive_mutex BatchLock;
    bool WorkerFailed = false;

    [[nodiscard]] bool RegisterWorker(std::shared_ptr<WorkerProcess> process);
    void UnregisterWorker(WorkerProcess* process) noexcept;
    void StopAllWorkers() noexcept;
    [[nodiscard]] bool IsExiting();

    const std::optional<std::string>& CurrentProcessPath()
    {
        static const std::optional<std::string> path = EnvironmentProcessPath();
        return path;
    }

    class WorkerProcess final
        : public std::enable_shared_from_this<WorkerProcess>
    {
    public:
        WorkerProcess() = default;
        WorkerProcess(const WorkerProcess&) = delete;
        WorkerProcess& operator=(const WorkerProcess&) = delete;
        WorkerProcess(WorkerProcess&&) = delete;
        WorkerProcess& operator=(WorkerProcess&&) = delete;
        ~WorkerProcess() { Stop(); }

        [[nodiscard]] bool HasExited()
        {
            const std::lock_guard<std::mutex> guard(_stateLock);
            return HasExitedCore();
        }

        [[nodiscard]] std::int32_t ExitCode()
        {
            const std::lock_guard<std::mutex> guard(_stateLock);
            if (!HasExitedCore()) return 1;
#ifdef _WIN32
            DWORD code = 1;
            if (_process == nullptr || !::GetExitCodeProcess(_process, &code)) return 1;
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(code));
#else
            if (!_exitStatusAvailable) return 1;
            if (WIFEXITED(_exitStatus)) return WEXITSTATUS(_exitStatus);
            if (WIFSIGNALED(_exitStatus)) return 128 + WTERMSIG(_exitStatus);
            return 1;
#endif
        }

        [[nodiscard]] std::uint32_t Id()
        {
#ifdef _WIN32
            const std::lock_guard<std::mutex> guard(_stateLock);
            return _process == nullptr ? 0 : ::GetProcessId(_process);
#else
            const std::lock_guard<std::mutex> guard(_stateLock);
            return _pid <= 0 ? 0 : static_cast<std::uint32_t>(_pid);
#endif
        }

        void Terminate() noexcept
        {
            const std::lock_guard<std::mutex> guard(_stateLock);
#ifdef _WIN32
            if (_process != nullptr) static_cast<void>(::TerminateProcess(_process, 1));
#else
            if (_pid > 0) static_cast<void>(::kill(_pid, SIGKILL));
#endif
        }

        [[nodiscard]] bool WaitForExit(std::chrono::milliseconds timeout) noexcept
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (true)
            {
                try
                {
                    if (HasExited()) return true;
                }
                catch (...)
                {
                    return false;
                }
                if (std::chrono::steady_clock::now() >= deadline) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        bool StartOutputReaders()
        {
            try
            {
                _standardOutputThread = std::thread([this] { DrainOutput(true); });
                _standardErrorThread = std::thread([this] { DrainOutput(false); });
                if (!RegisterWorker(shared_from_this()))
                {
                    Terminate();
                    static_cast<void>(WaitForExit(std::chrono::seconds(5)));
                    JoinOutputReaders();
                    return false;
                }
            }
            catch (...)
            {
                Terminate();
                static_cast<void>(WaitForExit(std::chrono::seconds(5)));
                JoinOutputReaders();
                throw;
            }
            return true;
        }

        void Dispose() noexcept { Stop(); }

        void Stop() noexcept
        {
            const std::lock_guard<std::mutex> guard(_stopLock);
            if (_stopped) return;
            _stopped = true;
            try
            {
                if (!HasExited()) Terminate();
            }
            catch (...)
            {
                Terminate();
            }
            if (!WaitForExit(std::chrono::seconds(5)))
            {
                Terminate();
                static_cast<void>(WaitForExit(std::chrono::seconds(5)));
            }
            JoinOutputReaders();
            {
                const std::lock_guard<std::mutex> stateGuard(_stateLock);
#ifdef _WIN32
                if (_standardOutput != nullptr)
                {
                    ::CloseHandle(_standardOutput);
                    _standardOutput = nullptr;
                }
                if (_standardError != nullptr)
                {
                    ::CloseHandle(_standardError);
                    _standardError = nullptr;
                }
                if (_process != nullptr)
                {
                    ::CloseHandle(_process);
                    _process = nullptr;
                }
#else
                if (_standardOutput >= 0)
                {
                    ::close(_standardOutput);
                    _standardOutput = -1;
                }
                if (_standardError >= 0)
                {
                    ::close(_standardError);
                    _standardError = -1;
                }
#endif
            }
            UnregisterWorker(this);
        }

    private:
        [[nodiscard]] bool HasExitedCore()
        {
#ifdef _WIN32
            if (_process == nullptr) return true;
            const DWORD wait = ::WaitForSingleObject(_process, 0);
            if (wait == WAIT_OBJECT_0)
            {
                return true;
            }
            if (wait == WAIT_TIMEOUT)
            {
                return false;
            }
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
#else
            if (_exited || _pid <= 0)
            {
                return true;
            }
            while (true)
            {
                int status = 0;
                const pid_t result = ::waitpid(_pid, &status, WNOHANG);
                if (result == _pid)
                {
                    _exited = true;
                    _exitStatus = status;
                    _exitStatusAvailable = true;
                    return true;
                }
                if (result == 0)
                {
                    return false;
                }
                if (result < 0 && errno == EINTR)
                {
                    continue;
                }
                if (result < 0 && errno == ECHILD)
                {
                    _exited = true;
                    return true;
                }
                throw std::system_error(errno, std::generic_category());
            }
#endif
        }

        void JoinOutputReaders() noexcept
        {
            try
            {
                if (_standardOutputThread.joinable()) _standardOutputThread.join();
                if (_standardErrorThread.joinable()) _standardErrorThread.join();
            }
            catch (...)
            {
            }
        }

        void DrainOutput(bool standardOutput) noexcept
        {
            std::array<char, 4096> buffer{};
            std::string line;
            bool truncated = false;
            std::size_t lineCodeUnits = 0;
            std::size_t continuationBytes = 0;
            auto consume = [&](const char* data, std::size_t length)
            {
                for (std::size_t i = 0; i < length; ++i)
                {
                    const char value = data[i];
                    if (value == '\n')
                    {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (_outputLines.fetch_add(1, std::memory_order_relaxed) < 64)
                        {
                            try { ThumbnailLog::Write(line); } catch (...) { }
                        }
                        line.clear();
                        truncated = false;
                        lineCodeUnits = 0;
                        continuationBytes = 0;
                    }
                    else if (truncated)
                    {
                        continue;
                    }
                    else
                    {
                        const auto byte = static_cast<unsigned char>(value);
                        if (continuationBytes > 0)
                        {
                            line.push_back(value);
                            --continuationBytes;
                            continue;
                        }
                        const std::size_t codeUnits =
                            byte >= 0xF0 && byte <= 0xF4 ? 2 : 1;
                        if (lineCodeUnits + codeUnits > 2048)
                        {
                            truncated = true;
                            continue;
                        }
                        line.push_back(value);
                        lineCodeUnits += codeUnits;
                        if (byte >= 0xC2 && byte <= 0xDF)
                        {
                            continuationBytes = 1;
                        }
                        else if (byte >= 0xE0 && byte <= 0xEF)
                        {
                            continuationBytes = 2;
                        }
                        else if (byte >= 0xF0 && byte <= 0xF4)
                        {
                            continuationBytes = 3;
                        }
                    }
                }
            };

            try
            {
                while (true)
                {
#ifdef _WIN32
                    HANDLE handle = standardOutput ? _standardOutput : _standardError;
                    DWORD count = 0;
                    if (handle == nullptr
                        || !::ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                            &count, nullptr)
                        || count == 0)
                    {
                        break;
                    }
                    consume(buffer.data(), static_cast<std::size_t>(count));
#else
                    const int handle = standardOutput ? _standardOutput : _standardError;
                    const ssize_t count = ::read(handle, buffer.data(), buffer.size());
                    if (count == 0) break;
                    if (count < 0)
                    {
                        if (errno == EINTR) continue;
                        break;
                    }
                    consume(buffer.data(), static_cast<std::size_t>(count));
#endif
                }
                if (!line.empty() || truncated)
                {
                    if (_outputLines.fetch_add(1, std::memory_order_relaxed) < 64)
                    {
                        try { ThumbnailLog::Write(line); } catch (...) { }
                    }
                }
            }
            catch (...)
            {
            }
        }

    public:

        static std::shared_ptr<WorkerProcess> Start(
            const std::string& exePath,
            const std::vector<std::string>& arguments,
            const std::filesystem::path& workingDirectory)
        {
            // Process.Start allocates its Process object before it creates OS
            // resources. Do the same so allocation failure cannot occur after
            // a worker has already been launched.
            auto result = std::make_shared<WorkerProcess>();

#ifdef _WIN32
            WindowsPipe output{};
            WindowsPipe error{};
            try
            {
                output = CreateRedirectPipe();
                error = CreateRedirectPipe();

                const std::wstring exe = Utf8ToWide(exePath);
                std::wstring command;
                command.reserve(exe.size() + 2 + arguments.size() * 8);
                command.push_back(L'"');
                command += exe;
                command.push_back(L'"');
                for (const std::string& argument : arguments)
                {
                    command.push_back(L' ');
                    command += PasteArgument(Utf8ToWide(argument));
                }
                std::vector<wchar_t> mutableCommand(command.begin(), command.end());
                mutableCommand.push_back(L'\0');

                STARTUPINFOW startup{};
                startup.cb = sizeof(startup);
                startup.dwFlags = STARTF_USESTDHANDLES;

                PROCESS_INFORMATION process{};
                HANDLE childOutput = nullptr;
                HANDLE childError = nullptr;
                HANDLE childInput = nullptr;
                bool closeChildOutput = false;
                bool closeChildError = false;
                bool closeChildInput = false;
                DWORD createError = ERROR_SUCCESS;

                {
                    const std::lock_guard<std::mutex> guard(WindowsProcessStartMutex);
                    try
                    {
                        childOutput =
                            DuplicateAsInheritable(output.Write, closeChildOutput);
                        childError =
                            DuplicateAsInheritable(error.Write, closeChildError);
                        childInput = DuplicateAsInheritable(
                            ::GetStdHandle(STD_INPUT_HANDLE), closeChildInput);

                        startup.hStdInput = childInput;
                        startup.hStdOutput = childOutput;
                        startup.hStdError = childError;

                        const std::wstring cwd = workingDirectory.native();
                        if (!::CreateProcessW(
                            nullptr,
                            mutableCommand.data(),
                            nullptr,
                            nullptr,
                            TRUE,
                            CREATE_NO_WINDOW,
                            nullptr,
                            cwd.c_str(),
                            &startup,
                            &process))
                        {
                            createError = ::GetLastError();
                        }
                    }
                    catch (...)
                    {
                        if (closeChildInput && childInput != nullptr
                            && childInput != INVALID_HANDLE_VALUE)
                        {
                            ::CloseHandle(childInput);
                        }
                        if (closeChildOutput && childOutput != nullptr
                            && childOutput != INVALID_HANDLE_VALUE)
                        {
                            ::CloseHandle(childOutput);
                        }
                        if (closeChildError && childError != nullptr
                            && childError != INVALID_HANDLE_VALUE)
                        {
                            ::CloseHandle(childError);
                        }
                        throw;
                    }

                    if (closeChildInput && childInput != nullptr
                        && childInput != INVALID_HANDLE_VALUE)
                    {
                        ::CloseHandle(childInput);
                    }
                    if (closeChildOutput && childOutput != nullptr
                        && childOutput != INVALID_HANDLE_VALUE)
                    {
                        ::CloseHandle(childOutput);
                    }
                    if (closeChildError && childError != nullptr
                        && childError != INVALID_HANDLE_VALUE)
                    {
                        ::CloseHandle(childError);
                    }
                }

                // Process.CloseChildHandles closes the parent's copies of the
                // child pipe handles immediately after CreateProcess returns.
                ::CloseHandle(output.Write);
                output.Write = nullptr;
                ::CloseHandle(error.Write);
                error.Write = nullptr;

                if (createError != ERROR_SUCCESS)
                {
                    throw ProcessStartFailure(
                        exePath, workingDirectory, WindowsErrorMessage(createError));
                }

                ::CloseHandle(process.hThread);
                result->_process = process.hProcess;
                result->_standardOutput = output.Read;
                result->_standardError = error.Read;
                output.Read = nullptr;
                error.Read = nullptr;
                if (!result->StartOutputReaders()) return nullptr;
                return result;
            }
            catch (...)
            {
                if (output.Read != nullptr && output.Read != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(output.Read);
                }
                if (output.Write != nullptr && output.Write != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(output.Write);
                }
                if (error.Read != nullptr && error.Read != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(error.Read);
                }
                if (error.Write != nullptr && error.Write != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(error.Write);
                }
                throw;
            }
#else
            int outputPipe[2]{-1, -1};
            int errorPipe[2]{-1, -1};
            int launchPipe[2]{-1, -1};

            auto closePair = [](int (&fds)[2]) noexcept
            {
                if (fds[0] >= 0) ::close(fds[0]);
                if (fds[1] >= 0) ::close(fds[1]);
                fds[0] = -1;
                fds[1] = -1;
            };

            auto createCloexecPipe = [&](int (&fds)[2])
            {
#if defined(__linux__) && defined(SYS_pipe2)
                if (::syscall(SYS_pipe2, fds, O_CLOEXEC) == 0)
                {
                    return;
                }
                if (errno != ENOSYS && errno != EINVAL)
                {
                    throw std::system_error(errno, std::generic_category());
                }
#endif
                if (::pipe(fds) != 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }
                for (int fd : fds)
                {
                    const int flags = ::fcntl(fd, F_GETFD);
                    if (flags < 0 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
                    {
                        const int failure = errno;
                        closePair(fds);
                        throw std::system_error(failure, std::generic_category());
                    }
                }
            };

            try
            {
                createCloexecPipe(outputPipe);
                createCloexecPipe(errorPipe);
            }
            catch (...)
            {
                closePair(outputPipe);
                closePair(errorPipe);
                throw;
            }

            std::string cwd;
            std::vector<char*> argv;
            try
            {
                cwd = workingDirectory.string();
                argv.reserve(arguments.size() + 2);
                argv.push_back(const_cast<char*>(exePath.c_str()));
                for (const std::string& argument : arguments)
                {
                    argv.push_back(const_cast<char*>(argument.c_str()));
                }
                argv.push_back(nullptr);

                std::error_code directoryError;
                if (std::filesystem::is_directory(exePath, directoryError)
                    && !directoryError)
                {
                    throw std::runtime_error(
                        "The FileName property should not be a directory unless UseShellExecute is set.");
                }

                createCloexecPipe(launchPipe);
            }
            catch (...)
            {
                closePair(outputPipe);
                closePair(errorPipe);
                closePair(launchPipe);
                throw;
            }

            sigset_t allSignals{};
            sigset_t oldSignals{};
            sigfillset(&allSignals);
            const int maskResult =
                ::pthread_sigmask(SIG_SETMASK, &allSignals, &oldSignals);
            if (maskResult != 0)
            {
                closePair(outputPipe);
                closePair(errorPipe);
                closePair(launchPipe);
                throw std::system_error(maskResult, std::generic_category());
            }

            const pid_t pid = ::fork();
            const int forkError = errno;
            if (pid != 0)
            {
                (void)::pthread_sigmask(SIG_SETMASK, &oldSignals, nullptr);
            }
            if (pid < 0)
            {
                closePair(outputPipe);
                closePair(errorPipe);
                closePair(launchPipe);
                throw ProcessStartFailure(
                    exePath, workingDirectory,
                    std::error_code(forkError, std::generic_category()).message());
            }

            if (pid == 0)
            {
                ::close(outputPipe[0]);
                ::close(errorPipe[0]);
                ::close(launchPipe[0]);

                auto launchFailure = [&](int errorCode) noexcept
                {
                    const int saved = errorCode;
                    const char* bytes = reinterpret_cast<const char*>(&saved);
                    std::size_t written = 0;
                    while (written < sizeof(saved))
                    {
                        const ssize_t count =
                            ::write(launchPipe[1], bytes + written, sizeof(saved) - written);
                        if (count > 0)
                        {
                            written += static_cast<std::size_t>(count);
                        }
                        else if (count < 0 && errno == EINTR)
                        {
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }
                    ::_exit(127);
                };

                // Caught handlers become SIG_DFL across exec. Reset them before
                // unblocking signals so no managed/native parent handler can run
                // in the fork child during its pre-exec setup.
                struct sigaction defaultAction{};
                defaultAction.sa_handler = SIG_DFL;
                sigemptyset(&defaultAction.sa_mask);
                for (int signal = 1; signal < NSIG; ++signal)
                {
                    if (signal == SIGKILL || signal == SIGSTOP)
                    {
                        continue;
                    }
                    struct sigaction current{};
                    if (::sigaction(signal, nullptr, &current) == 0
                        && current.sa_handler != SIG_DFL
                        && current.sa_handler != SIG_IGN)
                    {
                        (void)::sigaction(signal, &defaultAction, nullptr);
                    }
                }
                (void)::pthread_sigmask(SIG_SETMASK, &oldSignals, nullptr);

                if (::chdir(cwd.c_str()) != 0)
                {
                    launchFailure(errno);
                }
                if (outputPipe[1] != STDOUT_FILENO
                    && ::dup2(outputPipe[1], STDOUT_FILENO) < 0)
                {
                    launchFailure(errno);
                }
                if (errorPipe[1] != STDERR_FILENO
                    && ::dup2(errorPipe[1], STDERR_FILENO) < 0)
                {
                    launchFailure(errno);
                }
                if (outputPipe[1] != STDOUT_FILENO) ::close(outputPipe[1]);
                if (errorPipe[1] != STDERR_FILENO) ::close(errorPipe[1]);
                ::execv(exePath.c_str(), argv.data());
                launchFailure(errno);
            }

            ::close(outputPipe[1]);
            outputPipe[1] = -1;
            ::close(errorPipe[1]);
            errorPipe[1] = -1;
            ::close(launchPipe[1]);
            launchPipe[1] = -1;

            int launchError = 0;
            std::size_t received = 0;
            while (received < sizeof(launchError))
            {
                const ssize_t count = ::read(
                    launchPipe[0],
                    reinterpret_cast<char*>(&launchError) + received,
                    sizeof(launchError) - received);
                if (count > 0)
                {
                    received += static_cast<std::size_t>(count);
                    continue;
                }
                if (count == 0)
                {
                    break;
                }
                if (errno == EINTR)
                {
                    continue;
                }
                launchError = errno;
                received = sizeof(launchError);
                break;
            }
            ::close(launchPipe[0]);
            launchPipe[0] = -1;

            if (received == sizeof(launchError))
            {
                int ignored = 0;
                while (::waitpid(pid, &ignored, 0) < 0 && errno == EINTR)
                {
                }
                closePair(outputPipe);
                closePair(errorPipe);
                throw ProcessStartFailure(
                    exePath, workingDirectory,
                    std::error_code(launchError, std::generic_category()).message());
            }

            result->_pid = pid;
            result->_standardOutput = outputPipe[0];
            result->_standardError = errorPipe[0];
            outputPipe[0] = -1;
            errorPipe[0] = -1;
            if (!result->StartOutputReaders()) return nullptr;
            return result;
#endif
        }

    private:
        std::mutex _stateLock;
        std::mutex _stopLock;
        bool _stopped = false;
        std::thread _standardOutputThread;
        std::thread _standardErrorThread;
        std::atomic_int _outputLines{0};
#ifdef _WIN32
        HANDLE _process = nullptr;
        HANDLE _standardOutput = nullptr;
        HANDLE _standardError = nullptr;
#else
        pid_t _pid = -1;
        int _standardOutput = -1;
        int _standardError = -1;
        bool _exited = false;
        int _exitStatus = 0;
        bool _exitStatusAvailable = false;
#endif
    };

    bool RegisterWorker(std::shared_ptr<WorkerProcess> process)
    {
        const std::lock_guard<std::mutex> guard(ProcessLock);
        if (Exiting) return false;
        if (!ExitHandlerRegistered)
        {
            if (std::atexit(&StopAllWorkers) != 0) return false;
            ExitHandlerRegistered = true;
        }
        ActiveWorkers.push_back(std::move(process));
        return true;
    }

    void UnregisterWorker(WorkerProcess* process) noexcept
    {
        try
        {
            const std::lock_guard<std::mutex> guard(ProcessLock);
            ActiveWorkers.erase(
                std::remove_if(ActiveWorkers.begin(), ActiveWorkers.end(),
                    [&](const std::shared_ptr<WorkerProcess>& active)
                    {
                        return active.get() == process;
                    }),
                ActiveWorkers.end());
        }
        catch (...)
        {
        }
    }

    void StopAllWorkers() noexcept
    {
        std::vector<std::shared_ptr<WorkerProcess>> active;
        try
        {
            const std::lock_guard<std::mutex> guard(ProcessLock);
            Exiting = true;
            active = ActiveWorkers;
        }
        catch (...)
        {
            return;
        }
        for (const std::shared_ptr<WorkerProcess>& process : active)
        {
            if (process) process->Stop();
        }
    }

    bool IsExiting()
    {
        const std::lock_guard<std::mutex> guard(ProcessLock);
        return Exiting;
    }

    std::vector<std::vector<std::string>> Shares(
        const std::vector<std::string>& rooms,
        std::int32_t parallelism)
    {
        const std::int32_t workers = std::min(
            parallelism, static_cast<std::int32_t>(rooms.size()));
        std::vector<std::vector<std::string>> shares;
        shares.reserve(static_cast<std::size_t>(workers));
        for (std::int32_t i = 0; i < workers; ++i)
        {
            shares.emplace_back();
        }
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(rooms.size()); ++i)
        {
            shares[static_cast<std::size_t>(i % workers)].push_back(
                rooms[static_cast<std::size_t>(i)]);
        }
        return shares;
    }

    std::shared_ptr<WorkerProcess> StartWorker(
        const std::string& exePath,
        const std::vector<std::string>& share,
        std::int32_t width,
        std::int32_t height)
    {
        if (IsExiting()) return nullptr;
        const std::filesystem::path workingDirectory = std::filesystem::current_path();
        std::vector<std::string> arguments;
        for (const std::string& room : share)
        {
            arguments.emplace_back("-thumbnail");
            arguments.push_back(room);
        }
        arguments.emplace_back("-size");
        arguments.push_back(std::to_string(width) + "x" + std::to_string(height));

        try
        {
            return WorkerProcess::Start(exePath, arguments, workingDirectory);
        }
        catch (const std::exception& ex)
        {
            ThumbnailLog::Write(std::string("worker failed to start: ") + ex.what());
            return nullptr;
        }
    }

    std::int32_t RunWorkers(
        const std::vector<std::string>& rooms,
        std::int32_t parallelism,
        std::int32_t width,
        std::int32_t height,
        const std::string& exePath,
        MphRead::Mods::ThumbnailBatch::WorkerTimeout workerTimeout,
        const std::function<void(const std::string&)>& report,
        std::vector<std::string>& failedRooms,
        bool& abnormalExit)
    {
        failedRooms.clear();
        abnormalExit = false;
        std::vector<std::shared_ptr<WorkerProcess>> running;
        PendingSet pending(rooms);
        std::int32_t written = 0;

        auto reportCompleted = [&]()
        {
            for (const std::string& room : rooms)
            {
                if (!pending.Contains(room) || !ThumbnailGenerator::Exists(room))
                {
                    continue;
                }
                pending.Remove(room);
                ++written;
                const std::string line =
                    "[thumbnails] " + std::to_string(written) + "/"
                    + std::to_string(rooms.size()) + " ok " + room;
                if (report)
                {
                    report(line);
                }
            }
        };

        auto stopWorkers = [&]() noexcept
        {
            for (const std::shared_ptr<WorkerProcess>& process : running)
            {
                if (process)
                {
                    process->Stop();
                }
            }
        };

        try
        {
            const auto started = std::chrono::steady_clock::now();
            for (const std::vector<std::string>& share : Shares(rooms, parallelism))
            {
                std::shared_ptr<WorkerProcess> process =
                    StartWorker(exePath, share, width, height);
                if (!process)
                {
                    abnormalExit = true;
                    break;
                }
                running.push_back(std::move(process));
            }

            while (!abnormalExit)
            {
                bool allExited = true;
                for (const std::shared_ptr<WorkerProcess>& process : running)
                {
                    if (!process->HasExited())
                    {
                        allExited = false;
                        continue;
                    }
                    const std::int32_t exitCode = process->ExitCode();
                    if (exitCode != 0)
                    {
                        ThumbnailLog::Write(
                            "worker " + std::to_string(process->Id())
                            + " exited with code " + std::to_string(exitCode));
                        abnormalExit = true;
                    }
                }
                if (allExited || abnormalExit) break;
                if (std::chrono::steady_clock::now() - started >= workerTimeout)
                {
                    const auto seconds =
                        std::chrono::round<std::chrono::seconds>(workerTimeout).count();
                    ThumbnailLog::Write(
                        "worker timeout after " + std::to_string(seconds) + " seconds");
                    abnormalExit = true;
                    break;
                }
                reportCompleted();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        catch (...)
        {
            stopWorkers();
            throw;
        }
        stopWorkers();
        reportCompleted();
        for (const std::string& room : rooms)
        {
            if (!pending.Remove(room))
            {
                continue;
            }
            failedRooms.push_back(room);
            const std::string line = "[thumbnails] FAILED " + room;
            if (report)
            {
                report(line);
            }
            ThumbnailLog::Write(line);
        }
        return written;
    }

    std::int32_t RunSerial(
        const std::vector<std::string>& rooms,
        std::int32_t width,
        std::int32_t height,
        const std::function<void(const std::string&)>& report)
    {
        std::int32_t written = 0;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(rooms.size()); ++i)
        {
            const std::string& room = rooms[static_cast<std::size_t>(i)];
            if (ThumbnailGenerator::Exists(room))
            {
                continue;
            }
            if (ThumbnailCapture::CaptureRoom(room, width, height))
            {
                ++written;
            }
            const std::string line =
                "[thumbnails] " + std::to_string(i + 1) + "/"
                + std::to_string(rooms.size()) + "  " + room;
            std::cout << line << std::endl;
            if (report)
            {
                report(line);
            }
        }
        return written;
    }
}

namespace MphRead::Mods
{
    std::int32_t ThumbnailBatch::DefaultParallelism()
    {
#ifdef __APPLE__
        return 1;
#else
        return std::clamp(::MphRead::NativeRuntime::EnvironmentProcessorCount(), 2, 10);
#endif
    }

    bool ThumbnailBatch::CanRun()
    {
#ifdef __ANDROID__
        return false;
#else
        return CurrentProcessPath().has_value();
#endif
    }

    std::int32_t ThumbnailBatch::Run(
        const std::vector<std::string>& rooms,
        std::int32_t parallelism,
        std::int32_t width,
        std::int32_t height,
        const std::function<void(const std::string&)>& report,
        ThumbnailBatch::WorkerTimeout workerTimeout)
    {
        const std::lock_guard<std::recursive_mutex> batchGuard(BatchLock);
        if (WorkerFailed)
        {
            if (report)
            {
                report("Preview generation stopped after a worker failure; restart the app to retry.");
            }
            return 0;
        }

        std::vector<std::string> missing;
        missing.reserve(rooms.size());
        for (const std::string& room : rooms)
        {
            if (!ThumbnailGenerator::Exists(room))
            {
                missing.push_back(room);
            }
        }
        if (missing.empty())
        {
            return 0;
        }
        ThumbnailLog::Begin(static_cast<std::int32_t>(missing.size()));

#ifdef __APPLE__
        parallelism = 1;
#else
        parallelism = std::clamp(parallelism, 1, 16);
#endif
        if (workerTimeout <= ThumbnailBatch::WorkerTimeout::zero())
        {
            throw ::System::ArgumentOutOfRangeException("workerTimeout");
        }
        const std::optional<std::string> exePath = CurrentProcessPath();
        if (!exePath.has_value())
        {
            std::cout
                << "[thumbnails] cannot locate this executable; running serially"
                << std::endl;
            return RunSerial(missing, width, height, report);
        }

        std::vector<std::string> failed;
        bool abnormalExit = false;
        std::int32_t written = RunWorkers(
            missing, parallelism, width, height, *exePath, workerTimeout,
            report, failed, abnormalExit);
        if (abnormalExit)
        {
            WorkerFailed = true;
            const std::string note =
                "[thumbnails] worker failed; stopping previews without automatic retries";
            if (report)
            {
                report(note);
            }
            ThumbnailLog::Write(note);
        }
        else if (!failed.empty() && parallelism > 1)
        {
            const std::string note =
                "[thumbnails] " + std::to_string(failed.size())
                + " preview(s) missing; retrying in one worker";
            if (report)
            {
                report(note);
            }
            ThumbnailLog::Write(note);

            std::vector<std::string> retryFailed;
            bool retryAbnormal = false;
            written += RunWorkers(
                failed, 1, width, height, *exePath, workerTimeout,
                report, retryFailed, retryAbnormal);
            WorkerFailed = retryAbnormal;
        }
        return written;
    }
}
