#include "HashCode.hpp"

#include "Encoding.hpp"

#include <bit>
#include <random>

namespace MphRead::NativeRuntime::HashCodeDetail
{
    std::uint32_t Seed() noexcept
    {
        static const std::uint32_t seed = []() noexcept
        {
            try
            {
                std::random_device device;
                return std::uniform_int_distribution<std::uint32_t>()(device);
            }
            catch (...)
            {
                // No entropy source: a fixed seed still hashes consistently.
                return 0U;
            }
        }();
        return seed;
    }

    std::uint64_t StringSeed() noexcept
    {
        static const std::uint64_t seed = []() noexcept
        {
            try
            {
                std::random_device device;
                std::uniform_int_distribution<std::uint32_t> distribution;
                const std::uint64_t low = distribution(device);
                const std::uint64_t high = distribution(device);
                return low | (high << 32);
            }
            catch (...)
            {
                // No entropy source: use the same fixed-seed fallback as HashCode.
                return std::uint64_t{0};
            }
        }();
        return seed;
    }

    void MarvinBlock(std::uint32_t& p0, std::uint32_t& p1) noexcept
    {
        p1 ^= p0;
        p0 = std::rotl(p0, 20);
        p0 += p1;
        p1 = std::rotl(p1, 9);
        p1 ^= p0;
        p0 = std::rotl(p0, 27);
        p0 += p1;
        p1 = std::rotl(p1, 19);
    }
}

namespace MphRead::NativeRuntime
{
    std::int32_t StringGetHashCode(std::string_view value) noexcept
    {
        std::uint64_t seed = HashCodeDetail::StringSeed();
        std::uint32_t p0 = static_cast<std::uint32_t>(seed);
        std::uint32_t p1 = static_cast<std::uint32_t>(seed >> 32);
        std::uint32_t firstWord = 0;
        bool hasFirstWord = false;
        std::uint16_t firstUnit = 0;
        bool hasFirstUnit = false;
        const auto pushUnit = [&](std::uint16_t unit)
        {
            if (!hasFirstUnit)
            {
                firstUnit = unit;
                hasFirstUnit = true;
                return;
            }

            const std::uint32_t word = static_cast<std::uint32_t>(firstUnit)
                | (static_cast<std::uint32_t>(unit) << 16);
            hasFirstUnit = false;
            if (!hasFirstWord)
            {
                firstWord = word;
                hasFirstWord = true;
                return;
            }

            p0 += firstWord;
            HashCodeDetail::MarvinBlock(p0, p1);
            p0 += word;
            HashCodeDetail::MarvinBlock(p0, p1);
            hasFirstWord = false;
        };

        for (std::size_t offset = 0; offset < value.size();)
        {
            const Utf8Scalar scalar = DecodeUtf8Scalar(value, offset);
            char32_t code = scalar.Value;
            if (code >= 0x10000U)
            {
                code -= 0x10000U;
                pushUnit(static_cast<std::uint16_t>(0xD800U + (code >> 10)));
                pushUnit(static_cast<std::uint16_t>(0xDC00U + (code & 0x3FFU)));
            }
            else
            {
                pushUnit(static_cast<std::uint16_t>(code));
            }
            offset += scalar.Length;
        }

        // A remaining 4-byte word is consumed and blocked before the final
        // partial word, as Marvin.ComputeHash32 does. UTF-16 leaves 0 or 2
        // bytes after that; 0x80 is its mandatory final marker.
        if (hasFirstWord)
        {
            p0 += firstWord;
            HashCodeDetail::MarvinBlock(p0, p1);
        }
        const std::uint32_t partial = hasFirstUnit
            ? (0x00800000U | static_cast<std::uint32_t>(firstUnit))
            : 0x80U;
        p0 += partial;
        HashCodeDetail::MarvinBlock(p0, p1);
        HashCodeDetail::MarvinBlock(p0, p1);
        return std::bit_cast<std::int32_t>(p0 ^ p1);
    }

    std::int32_t ReferenceGetHashCode(const void* value) noexcept
    {
        if (value == nullptr)
        {
            return 0;
        }
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
        return HashCodeCombine(static_cast<std::int32_t>(address), static_cast<std::int32_t>(address >> 32));
    }
}
