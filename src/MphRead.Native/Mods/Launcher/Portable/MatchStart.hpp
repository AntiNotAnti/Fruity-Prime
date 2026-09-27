#pragma once

#include <memory>

namespace MphRead
{
    class MenuSettings;
    class RenderWindow;
}

namespace MphRead::Mods::Launcher
{
    class LaunchPlan;

    class MatchStart final
    {
    public:
        MatchStart() = delete;

        static void Launch(
            std::shared_ptr<MphRead::MenuSettings> settings,
            LaunchPlan plan);
        [[nodiscard]] static bool Begin(
            MphRead::RenderWindow& window,
            std::shared_ptr<MphRead::MenuSettings> settings,
            LaunchPlan plan);
        static void AfterMatch();
        static void CommitAdventureSave();

    private:
        static void EnsureScene(MphRead::RenderWindow& window);
        [[nodiscard]] static bool BeginAdventure(
            MphRead::RenderWindow& window, LaunchPlan plan);
        [[nodiscard]] static bool BeginDemo(
            MphRead::RenderWindow& window, LaunchPlan plan);
        static void AddLocalPlayers(
            MphRead::RenderWindow& renderer,
            LaunchPlan plan,
            bool teamPlay);
    };
}
