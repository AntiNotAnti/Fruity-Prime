#pragma once

namespace MphRead::Mods::Diagnostics
{
    class CompatibilityCheck final
    {
    public:
        CompatibilityCheck() = delete;

        [[nodiscard]] static int Run();
    };
}
