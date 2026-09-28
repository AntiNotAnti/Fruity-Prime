#pragma once

#if defined(MPHREAD_SHELL)
namespace MphRead::Mods::Diagnostics
{
    class GlfwPathCheck final
    {
    public:
        GlfwPathCheck() = delete;

        [[nodiscard]] static int Run();
    };
}
#endif
