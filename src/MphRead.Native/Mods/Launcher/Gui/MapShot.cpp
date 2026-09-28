#include "MapShot.hpp"

#include "../../ThumbnailGenerator.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"

#include <exception>
#include <map>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using Bitmap = ::MphRead::NativeRuntime::Avalonia::Media::Imaging::Bitmap;

    namespace
    {
        // StringComparer.OrdinalIgnoreCase.
        using CacheMap = std::map<std::string, std::shared_ptr<Bitmap>, Runtime::OrdinalIgnoreCaseLess>;

        [[nodiscard]] CacheMap& Cache()
        {
            static CacheMap cache;
            return cache;
        }
    }

    std::shared_ptr<Bitmap> MapShot::For(const std::optional<std::string>& roomKey)
    {
        if (!roomKey.has_value() || roomKey->empty())
        {
            return nullptr;
        }
        auto& cache = Cache();
        const auto found = cache.find(*roomKey);
        if (found != cache.end())
        {
            return found->second;
        }
        std::shared_ptr<Bitmap> shot;
        try
        {
            const std::string path = ThumbnailGenerator::PathFor(*roomKey);
            if (Runtime::FileExists(path))
            {
                // Read whole, so the file is not held open while the preview
                // generator rewrites it.
                shot = Bitmap::FromBytes(Runtime::FileReadAllBytes(path));
            }
        }
        catch (const std::exception&)
        {
            // A truncated PNG from an interrupted batch is no picture.
        }
        cache[*roomKey] = shot;
        return shot;
    }

    void MapShot::Forget()
    {
        Cache().clear();
    }
}
