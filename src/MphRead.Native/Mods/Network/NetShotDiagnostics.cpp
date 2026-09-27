#include "NetShotDiagnostics.hpp"

#include "NetDamage.hpp"
#include "NetLog.hpp"
#include "NetPlayerBridge.hpp"
#include "NetPlayerLifecycle.hpp"
#include "NetSession.hpp"
#include "NetUnlagged.hpp"

#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Enum.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;

    std::string ToString(ShotAttemptResult value)
    {
        static constexpr std::array<Runtime::EnumNameEntry, 10> Names = {{
            {0, "Spawned"}, {1, "AttachedEnemy"}, {2, "Cooldown"}, {3, "AutofireCooldown"},
            {4, "GunLowered"}, {5, "NoAmmo"}, {6, "NoProjectileSlot"}, {7, "DeadOrNotInPlay"},
            {8, "StaleLife"}, {9, "OtherNoSpawn"}}};
        return Runtime::ManagedEnumToString(value, Names.data(), Names.size(), false);
    }

    ShotKey ShotKey::For(std::int32_t slot, std::uint32_t frame)
    {
        return ShotKey(NetSession::AuthorityEpoch(), NetSession::CurrentMatchId(), slot,
            NetPlayerLifecycle::Generation(slot), NetPlayerLifecycle::Get(slot), frame);
    }

    std::string ShotKey::ToString() const
    {
        return Runtime::ToString(AuthorityEpoch) + "/" + Runtime::ToString(MatchId) + "/"
            + Runtime::ToString(ShooterSlot) + "/" + Runtime::ToString(Generation) + "/"
            + Runtime::ToString(LifeId) + "/" + Runtime::ToString(LaunchFrame);
    }

    std::array<std::array<std::int64_t, NetShotDiagnostics::ResultCount>, NetShotDiagnostics::WeaponCount>
        NetShotDiagnostics::Outcomes{};
    NetShotDiagnostics::Counts NetShotDiagnostics::LocalHits{};
    NetShotDiagnostics::Counts NetShotDiagnostics::AuthorityHits{};
    NetShotDiagnostics::Counts NetShotDiagnostics::Predictions{};
    NetShotDiagnostics::Counts NetShotDiagnostics::Claims{};
    NetShotDiagnostics::Counts NetShotDiagnostics::Rescues{};
    NetShotDiagnostics::Counts NetShotDiagnostics::Refusals{};
    NetShotDiagnostics::Counts NetShotDiagnostics::PredictedDamage{};
    NetShotDiagnostics::Counts NetShotDiagnostics::AuthorityDamage{};
    NetShotDiagnostics::Counts NetShotDiagnostics::LocalHeadshots{};
    NetShotDiagnostics::Counts NetShotDiagnostics::AuthorityHeadshots{};
    NetShotDiagnostics::Counts NetShotDiagnostics::RewindSamples{};
    NetShotDiagnostics::Counts NetShotDiagnostics::RewindFrames{};
    NetShotDiagnostics::Counts NetShotDiagnostics::RewindClamps{};
    NetShotDiagnostics::Counts NetShotDiagnostics::ContinuousTicks{};
    NetShotDiagnostics::Counts NetShotDiagnostics::ContinuousDamageTicks{};
    NetShotDiagnostics::Counts NetShotDiagnostics::ContinuousAcquired{};
    NetShotDiagnostics::Counts NetShotDiagnostics::ContinuousAmmo{};
    NetShotDiagnostics::Counts NetShotDiagnostics::DrainCredit{};

    void NetShotDiagnostics::Continuous(::MphRead::Entities::BeamProjectileEntity& beam, std::int32_t ammo)
    {
        const auto b = static_cast<std::size_t>(Bucket(beam.Beam()));
        Runtime::IncrementInPlace(ContinuousTicks[b]);
        if (beam.Damage() > 0)
        {
            Runtime::IncrementInPlace(ContinuousDamageTicks[b]);
        }
        if (beam.Target() != nullptr)
        {
            Runtime::IncrementInPlace(ContinuousAcquired[b]);
        }
        ContinuousAmmo[b] = Runtime::UncheckedAdd(ContinuousAmmo[b], ammo);
        if (NetLog::Enabled())
        {
            Trace("continuous", beam.ModLaunchKey(), beam.Beam(),
                "phase=" + Runtime::ToString(beam.ModContinuousPhase) + " damage="
                + Runtime::ToString(beam.Damage()) + " ammo=" + Runtime::ToString(ammo)
                + " acquired=" + (beam.Target() != nullptr ? "True" : "False"));
        }
    }

    std::int32_t NetShotDiagnostics::Bucket(::MphRead::BeamType weapon) noexcept
    {
        const auto value = static_cast<std::int32_t>(weapon);
        return value >= 0 && value < WeaponCount - 1 ? value : WeaponCount - 1;
    }

    void NetShotDiagnostics::Trace(std::string_view stage, const ShotKey& key,
        ::MphRead::BeamType weapon, std::string_view detail)
    {
        if (NetLog::Enabled())
        {
            NetLog::Event("[shot] key=" + key.ToString() + " stage=" + std::string(stage)
                + " weapon=" + ::MphRead::ToString(weapon) + " authorityFrame="
                + Runtime::ToString(NetSession::NetFrame()) + " " + std::string(detail));
        }
    }

    bool NetShotDiagnostics::Finish(::MphRead::Entities::PlayerEntity& shooter,
        ShotAttemptResult result, OpenTK::Mathematics::Vector3 shot, OpenTK::Mathematics::Vector3 aim)
    {
        if (NetSession::Active())
        {
            Runtime::IncrementInPlace(Outcomes[static_cast<std::size_t>(Bucket(shooter.CurrentWeapon()))]
                [static_cast<std::size_t>(result)]);
            if (result == ShotAttemptResult::Spawned)
            {
                NetDamage::NoteFired(shooter, shot, aim);
            }
            if (NetLog::Enabled())
            {
                const std::int32_t slot = shooter.SlotIndex();
                Trace("attempt", ShotKey::For(slot, NetUnlagged::LaunchFrameFor(shooter)),
                    shooter.CurrentWeapon(), "result=" + ToString(result)
                    + " health=" + Runtime::ToString(shooter.Health())
                    + " shoot=" + (shooter.Controls().Shoot().IsDown() ? "True" : "False")
                    + " press=" + (shooter.Controls().Shoot().IsPressed() ? "True" : "False")
                    + " pressAge=" + Runtime::ToString(Runtime::ManagedAt(NetPlayerBridge::ShootPressAge, slot)));
            }
        }
        return result == ShotAttemptResult::Spawned;
    }

    void NetShotDiagnostics::Reset()
    {
        for (auto& row : Outcomes)
        {
            row.fill(0);
        }
        for (Counts* values : {&LocalHits, &AuthorityHits, &Predictions, &Claims, &Rescues, &Refusals,
                 &PredictedDamage, &AuthorityDamage, &LocalHeadshots, &AuthorityHeadshots, &RewindSamples,
                 &RewindFrames, &RewindClamps, &ContinuousTicks, &ContinuousDamageTicks, &ContinuousAcquired,
                 &ContinuousAmmo, &DrainCredit})
        {
            values->fill(0);
        }
    }

    std::string NetShotDiagnostics::Describe()
    {
        std::string text = "weapon attempted spawned localHits authorityHits predictions claims rescues "
            "refusals predictedDamage authorityDamage localHeadshots authorityHeadshots meanRewind clamp%\n";
        for (std::int32_t b = 0; b < WeaponCount; b++)
        {
            const auto i = static_cast<std::size_t>(b);
            std::int64_t attempted = 0;
            for (std::size_t r = 0; r < ResultCount; r++)
            {
                attempted = Runtime::UncheckedAdd(attempted, Outcomes[i][r]);
            }
            const std::int64_t observed = Runtime::UncheckedAdd(
                Runtime::UncheckedAdd(Runtime::UncheckedAdd(attempted, LocalHits[i]), AuthorityHits[i]), Claims[i]);
            if (observed == 0)
            {
                continue;
            }
            const double meanRewind = RewindSamples[i] == 0 ? 0.0
                : static_cast<double>(RewindFrames[i]) / static_cast<double>(RewindSamples[i]);
            const double clamp = RewindSamples[i] == 0 ? 0.0
                : 100.0 * static_cast<double>(RewindClamps[i]) / static_cast<double>(RewindSamples[i]);
            text += (b == WeaponCount - 1 ? std::string("alt/bomb")
                : ::MphRead::ToString(static_cast<::MphRead::BeamType>(b)))
                + " " + Runtime::ToString(attempted) + " " + Runtime::ToString(Outcomes[i][0])
                + " " + Runtime::ToString(LocalHits[i]) + " " + Runtime::ToString(AuthorityHits[i])
                + " " + Runtime::ToString(Predictions[i]) + " " + Runtime::ToString(Claims[i])
                + " " + Runtime::ToString(Rescues[i]) + " " + Runtime::ToString(Refusals[i])
                + " " + Runtime::ToString(PredictedDamage[i]) + " " + Runtime::ToString(AuthorityDamage[i])
                + " " + Runtime::ToString(LocalHeadshots[i]) + " " + Runtime::ToString(AuthorityHeadshots[i])
                + " " + Runtime::ToString(meanRewind, "F2") + " " + Runtime::ToString(clamp, "F1")
                + std::string(Runtime::EnvironmentNewLine());
            if (ContinuousTicks[i] > 0)
            {
                text += "  continuous ticks=" + Runtime::ToString(ContinuousTicks[i])
                    + " nonzero=" + Runtime::ToString(ContinuousDamageTicks[i])
                    + " acquired=" + Runtime::ToString(ContinuousAcquired[i])
                    + " ammo=" + Runtime::ToString(ContinuousAmmo[i])
                    + " drainCredit=" + Runtime::ToString(DrainCredit[i]) + std::string(Runtime::EnvironmentNewLine());
            }
            for (std::size_t r = 1; r < ResultCount; r++)
            {
                if (Outcomes[i][r] != 0)
                {
                    text += "  " + ToString(static_cast<ShotAttemptResult>(r)) + "="
                        + Runtime::ToString(Outcomes[i][r]) + std::string(Runtime::EnvironmentNewLine());
                }
            }
        }
        return text;
    }
}
