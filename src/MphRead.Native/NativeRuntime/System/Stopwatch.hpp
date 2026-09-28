#pragma once

// System.Diagnostics.Stopwatch's static members: the monotonic timestamp and
// how many of its ticks make a second (QueryPerformanceCounter on Windows,
// nanoseconds of CLOCK_MONOTONIC elsewhere).

#include <compare>
#include <cstdint>

namespace MphRead::NativeRuntime
{
    // Stopwatch.GetTimestamp().
    [[nodiscard]] std::int64_t StopwatchGetTimestamp() noexcept;
    // Stopwatch.Frequency.
    [[nodiscard]] std::int64_t StopwatchFrequency() noexcept;
    // Stopwatch.GetElapsedTime(start, end), as TimeSpan ticks: the tick
    // difference scaled to 100-nanosecond units and truncated, as .NET does.
    [[nodiscard]] std::int64_t StopwatchGetElapsedTicks(std::int64_t start, std::int64_t end) noexcept;
    // Stopwatch.GetElapsedTime(start) -- against the timestamp now.
    [[nodiscard]] std::int64_t StopwatchGetElapsedTicks(std::int64_t start) noexcept;
    // TimeSpan.TotalMilliseconds of a TimeSpan with this many ticks.
    [[nodiscard]] constexpr double TimeSpanTotalMilliseconds(std::int64_t ticks) noexcept
    {
        return static_cast<double>(ticks) / 10000.0;
    }

    // System.TimeSpan, as far as a stopwatch reading needs it: a count of
    // hundred-nanosecond ticks.
    struct TimeSpan final
    {
        std::int64_t Ticks = 0;

        [[nodiscard]] static constexpr TimeSpan FromSeconds(double seconds) noexcept
        {
            return TimeSpan{static_cast<std::int64_t>(seconds * 10000000.0)};
        }
        [[nodiscard]] static constexpr TimeSpan FromMilliseconds(double milliseconds) noexcept
        {
            return TimeSpan{static_cast<std::int64_t>(milliseconds * 10000.0)};
        }
        [[nodiscard]] constexpr double TotalSeconds() const noexcept { return static_cast<double>(Ticks) / 10000000.0; }
        [[nodiscard]] constexpr double TotalMilliseconds() const noexcept { return static_cast<double>(Ticks) / 10000.0; }
        friend constexpr TimeSpan operator-(TimeSpan a, TimeSpan b) noexcept { return {a.Ticks - b.Ticks}; }
        friend constexpr TimeSpan operator+(TimeSpan a, TimeSpan b) noexcept { return {a.Ticks + b.Ticks}; }
        friend constexpr auto operator<=>(const TimeSpan&, const TimeSpan&) noexcept = default;
    };

    // System.Diagnostics.Stopwatch.
    class Stopwatch final
    {
    public:
        [[nodiscard]] static Stopwatch StartNew() noexcept
        {
            Stopwatch watch;
            watch.Start();
            return watch;
        }

        void Start() noexcept
        {
            if (!_running)
            {
                _started = StopwatchGetTimestamp();
                _running = true;
            }
        }
        void Stop() noexcept
        {
            if (_running)
            {
                _elapsed += StopwatchGetTimestamp() - _started;
                _running = false;
            }
        }
        void Reset() noexcept
        {
            _elapsed = 0;
            _running = false;
        }
        void Restart() noexcept
        {
            _elapsed = 0;
            _started = StopwatchGetTimestamp();
            _running = true;
        }
        [[nodiscard]] bool IsRunning() const noexcept { return _running; }
        [[nodiscard]] std::int64_t ElapsedTicks() const noexcept
        {
            return _elapsed + (_running ? StopwatchGetTimestamp() - _started : 0);
        }
        [[nodiscard]] TimeSpan Elapsed() const noexcept
        {
            return TimeSpan{static_cast<std::int64_t>(static_cast<double>(ElapsedTicks()) * 10000000.0
                / static_cast<double>(StopwatchFrequency()))};
        }
        [[nodiscard]] std::int64_t ElapsedMilliseconds() const noexcept
        {
            return static_cast<std::int64_t>(Elapsed().TotalMilliseconds());
        }

    private:
        std::int64_t _started = 0;
        std::int64_t _elapsed = 0;
        bool _running = false;
    };
}
