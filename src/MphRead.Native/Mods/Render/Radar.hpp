#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>

namespace MphRead
{
    enum class ItemType : std::int32_t;
}

namespace MphRead::Mods::Render
{
    // The round motion-tracker overlay's settings and colours. Nothing here is
    // cut from a DS sprite: every shape is drawn flat.
    class Radar final
    {
    public:
        Radar() = delete;

        inline static bool Enabled = true;
        inline static bool ShowBackground = false;
        inline static bool ShowOutlines = true;
        static constexpr float Range = 24;

        [[nodiscard]] static bool IsWeaponItem(ItemType type) noexcept;

        struct Palette
        {
            Palette() = default;
            Palette(OpenTK::Mathematics::Vector4 background, OpenTK::Mathematics::Vector4 ring,
                OpenTK::Mathematics::Vector4 cone, OpenTK::Mathematics::Vector4 player,
                OpenTK::Mathematics::Vector4 hunter, OpenTK::Mathematics::Vector4 weapon,
                OpenTK::Mathematics::Vector4 powerup) noexcept
                : _background(background), _ring(ring), _cone(cone), _player(player),
                  _hunter(hunter), _weapon(weapon), _powerup(powerup)
            {
            }

            [[nodiscard]] OpenTK::Mathematics::Vector4 Background() const noexcept { return _background; }
            [[nodiscard]] OpenTK::Mathematics::Vector4 Ring() const noexcept { return _ring; }
            [[nodiscard]] OpenTK::Mathematics::Vector4 Cone() const noexcept { return _cone; }
            [[nodiscard]] OpenTK::Mathematics::Vector4 Player() const noexcept { return _player; }
            [[nodiscard]] OpenTK::Mathematics::Vector4 Hunter() const noexcept { return _hunter; }
            [[nodiscard]] OpenTK::Mathematics::Vector4 Weapon() const noexcept { return _weapon; }
            [[nodiscard]] OpenTK::Mathematics::Vector4 Powerup() const noexcept { return _powerup; }

        private:
            OpenTK::Mathematics::Vector4 _background{};
            OpenTK::Mathematics::Vector4 _ring{};
            OpenTK::Mathematics::Vector4 _cone{};
            OpenTK::Mathematics::Vector4 _player{};
            OpenTK::Mathematics::Vector4 _hunter{};
            OpenTK::Mathematics::Vector4 _weapon{};
            OpenTK::Mathematics::Vector4 _powerup{};
        };

        static const Palette PaletteOf;
    };
}
