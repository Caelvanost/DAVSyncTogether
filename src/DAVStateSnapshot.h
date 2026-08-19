#pragma once

#include "PCH.h"

namespace DAVSyncTogether
{
    enum class ArmorVisualState : std::uint8_t
    {
        Unknown = 0,
        Visible,
        Hidden,
        Replaced
    };

    struct WornArmorState
    {
        RE::FormID formID{ 0 };
        std::string editorID;
        std::string name;
        ArmorVisualState visualState{ ArmorVisualState::Unknown };
        std::vector<RE::FormID> baseArmorAddons;
        std::vector<RE::FormID> activeArmorAddons;

        [[nodiscard]] bool VisualEquivalent(const WornArmorState& rhs) const noexcept;
    };

    struct DAVStateSnapshot
    {
        bool davLoaded{ false };
        std::string playerName;
        std::vector<WornArmorState> wornArmors;

        [[nodiscard]] std::uint64_t StateHash() const noexcept;
        [[nodiscard]] std::string Summary() const;
    };

    [[nodiscard]] std::string_view ArmorVisualStateName(ArmorVisualState state) noexcept;
}
