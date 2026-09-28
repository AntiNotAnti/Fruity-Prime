#pragma once

#include <cmath>
#include <cstdint>

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssistTuning final
    {
    public:
        AimAssistTuning() = delete;

        static constexpr float AcquireCone = 7;
        static constexpr float ReleaseCone = 9;
        static constexpr float InnerCone = 2.4F;
        static constexpr float MinimumFriction = .62F;
        static constexpr float HorizontalRotation = .24F;
        static constexpr float VerticalRotation = .15F;
        static constexpr float ChallengerRatio = 1.30F;
        static constexpr float HeadDelay = .120F;
        static constexpr float MaxHeadBlend = .80F;
    };

    enum class AimAssistWeaponClass : std::int32_t { Standard, Tracking, Precision, Projectile, Splash };

    struct AimAssistWeaponProfile
    {
    private:
        float _cone = 0;
        float _releaseCone = 0;
        float _inner = 0;
        float _rotation = 0;
        float _maxSpeed = 0;
        bool _head = false;

    public:
        AimAssistWeaponProfile() = default;
        AimAssistWeaponProfile(float cone, float releaseCone, float inner, float rotation, float maxSpeed, bool head) noexcept
            : _cone(cone), _releaseCone(releaseCone), _inner(inner), _rotation(rotation), _maxSpeed(maxSpeed), _head(head)
        {
        }

        [[nodiscard]] float Cone() const noexcept { return _cone; }
        [[nodiscard]] float ReleaseCone() const noexcept { return _releaseCone; }
        [[nodiscard]] float Inner() const noexcept { return _inner; }
        [[nodiscard]] float Rotation() const noexcept { return _rotation; }
        [[nodiscard]] float MaxSpeed() const noexcept { return _maxSpeed; }
        [[nodiscard]] bool Head() const noexcept { return _head; }

        [[nodiscard]] static AimAssistWeaponProfile For(AimAssistWeaponClass weapon, bool scoped) noexcept
        {
            if (scoped)
            {
                return {3.5F, 4.75F, 1.25F, .5F, 8,
                    weapon == AimAssistWeaponClass::Standard || weapon == AimAssistWeaponClass::Precision};
            }
            switch (weapon)
            {
            case AimAssistWeaponClass::Tracking: return {7, 9, 2.4F, 1, 24, false};
            case AimAssistWeaponClass::Precision: return {5, 7, 1.8F, .6F, 12, true};
            case AimAssistWeaponClass::Splash: return {7, 9, 2.4F, .45F, 12, false};
            case AimAssistWeaponClass::Projectile: return {7, 9, 2.4F, .6F, 16, false};
            default: return {7, 9, 2.4F, .8F, 20, true};
            }
        }

        friend bool operator==(const AimAssistWeaponProfile& left, const AimAssistWeaponProfile& right) noexcept
        {
            const auto equals = [](float a, float b) noexcept
            {
                return a == b || (std::isnan(a) && std::isnan(b));
            };
            return equals(left._cone, right._cone) && equals(left._releaseCone, right._releaseCone)
                && equals(left._inner, right._inner) && equals(left._rotation, right._rotation)
                && equals(left._maxSpeed, right._maxSpeed) && left._head == right._head;
        }
    };
}
