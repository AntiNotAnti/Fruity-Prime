#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <ratio>
#include <string>
#include <vector>

namespace MphRead::Mods
{
    class ThumbnailBatch final
    {
    public:
        using WorkerTimeout = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

        ThumbnailBatch() = delete;
        ThumbnailBatch(const ThumbnailBatch&) = delete;
        ThumbnailBatch& operator=(const ThumbnailBatch&) = delete;

        [[nodiscard]] static std::int32_t DefaultParallelism();
        [[nodiscard]] static bool CanRun();
        static std::int32_t Run(
            const std::vector<std::string>& rooms,
            std::int32_t parallelism,
            std::int32_t width,
            std::int32_t height,
            const std::function<void(const std::string&)>& report = {},
            WorkerTimeout workerTimeout = std::chrono::minutes(5));
    };
}
