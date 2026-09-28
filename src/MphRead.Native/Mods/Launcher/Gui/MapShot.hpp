#pragma once

#include "../../../NativeRuntime/Avalonia/Media.hpp"

#include <memory>
#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // The launcher's own map renders, cached, for the screens that want one
    // behind something rather than beside it. Nothing is generated here.
    class MapShot final
    {
    public:
        MapShot() = delete;

        [[nodiscard]] static std::shared_ptr<::MphRead::NativeRuntime::Avalonia::Media::Imaging::Bitmap> For(
            const std::optional<std::string>& roomKey);
        // Drop the lot: the thumbnail pass has rewritten them.
        static void Forget();
    };
}
