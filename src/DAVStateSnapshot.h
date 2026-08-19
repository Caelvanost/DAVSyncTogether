#pragma once

#include "PCH.h"

namespace DAVSyncTogether
{
    struct WornArmorState
    {
        RE::FormID formID{ 0 };
        std::string editorID;
        std::string name;
    };

    struct DAVStateSnapshot
    {
        bool davLoaded{ false };
        std::string playerName;
        std::vector<WornArmorState> wornArmors;
        std::vector<std::string> sceneNodes;
        std::uint64_t sceneHash{ 0 };

        [[nodiscard]] std::uint64_t StateHash() const noexcept;
        [[nodiscard]] std::string Summary() const;
    };
}
