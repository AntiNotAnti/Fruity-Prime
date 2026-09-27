#pragma once

#if defined(MPHREAD_SHELL)
namespace MphRead::Mods::Diagnostics
{
    class ThumbnailWindowCheck final
    {
    public:
        ThumbnailWindowCheck() = delete;

        [[nodiscard]] static int Run(bool legacyCheck = false);
    };
}
#endif
