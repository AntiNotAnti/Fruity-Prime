#pragma once

#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace MphRead::Mods::Input
{
    struct StickCalibration
    {
    public:
        StickCalibration() = default;
        constexpr StickCalibration(float centerX, float centerY, float minX, float maxX,
            float minY, float maxY) noexcept
            : _centerX(centerX), _centerY(centerY), _minX(minX), _maxX(maxX), _minY(minY), _maxY(maxY)
        {
        }

        [[nodiscard]] float CenterX() const noexcept { return _centerX; }
        [[nodiscard]] float CenterY() const noexcept { return _centerY; }
        [[nodiscard]] float MinX() const noexcept { return _minX; }
        [[nodiscard]] float MaxX() const noexcept { return _maxX; }
        [[nodiscard]] float MinY() const noexcept { return _minY; }
        [[nodiscard]] float MaxY() const noexcept { return _maxY; }

        [[nodiscard]] static constexpr StickCalibration Default() noexcept
        {
            return StickCalibration{0, 0, -1, 1, -1, 1};
        }

        [[nodiscard]] std::pair<float, float> Normalize(float x, float y) const noexcept
        {
            return {Axis(x, _centerX, _minX, _maxX), Axis(y, _centerY, _minY, _maxY)};
        }

        friend bool operator==(const StickCalibration& left, const StickCalibration& right) noexcept
        {
            return Equal(left._centerX, right._centerX) && Equal(left._centerY, right._centerY)
                && Equal(left._minX, right._minX) && Equal(left._maxX, right._maxX)
                && Equal(left._minY, right._minY) && Equal(left._maxY, right._maxY);
        }

    private:
        float _centerX = 0.0F;
        float _centerY = 0.0F;
        float _minX = 0.0F;
        float _maxX = 0.0F;
        float _minY = 0.0F;
        float _maxY = 0.0F;

        [[nodiscard]] static bool Equal(float left, float right) noexcept
        {
            return left == right || (std::isnan(left) && std::isnan(right));
        }

        [[nodiscard]] static float Axis(float value, float center, float min, float max) noexcept
        {
            return std::clamp((value - center) /
                    ::MphRead::NativeRuntime::MathMax(.1F, value >= center ? max - center : center - min),
                -1.0F, 1.0F);
        }
    };
}
