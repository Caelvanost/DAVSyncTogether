#pragma once

#include "DAVStateSnapshot.h"

namespace DAVSyncTogether
{
    enum class NetworkArmorState : std::uint8_t
    {
        Visible = 1,
        Hidden,
        Replaced,
        Unequipped
    };

    struct RemoteArmorState
    {
        std::string sender;
        FormIdentity armor;
        NetworkArmorState state{ NetworkArmorState::Visible };
        std::vector<FormIdentity> activeArmorAddons;
    };

    [[nodiscard]] std::string EncodeArmorState(
        const WornArmorState& armor,
        bool unequipped = false);

    [[nodiscard]] std::optional<RemoteArmorState> DecodeArmorState(
        std::string_view packet);

    [[nodiscard]] std::string_view NetworkArmorStateName(
        NetworkArmorState state) noexcept;
}
