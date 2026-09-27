#include "GlNames.hpp"

#include <atomic>

namespace MphRead::Mods::Render
{
    namespace
    {
        std::atomic<std::int32_t> g_next{2'000'000};
    }

    std::int32_t GlNames::NextTexture() noexcept
    {
        return ++g_next;
    }
}
