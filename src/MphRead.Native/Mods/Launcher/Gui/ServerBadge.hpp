#pragma once

#include "Flags.hpp"

#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // The address badge drawn at the leading edge of a server row.
    class ServerBadge final
    {
    public:
        ServerBadge() = delete;

        enum class Kind
        {
            Internet,
            Lan,
            Local,
            Silent
        };

        static constexpr double Width = Flags::Width;
        static constexpr double Height = Flags::Height;

        [[nodiscard]] static std::string CountryOf(const std::string& endpoint);
        [[nodiscard]] static Kind Of(const std::string& endpoint, bool answered);
        static void Draw(Av::Media::DrawingContext& context, const std::string& endpoint,
            bool answered, double x, double y);
        static void Draw(Av::Media::DrawingContext& context, Kind kind, double x, double y);
    };
}
