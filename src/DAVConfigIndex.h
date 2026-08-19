#pragma once

#include "DAVStateSnapshot.h"

namespace DAVSyncTogether
{
    class DAVConfigIndex
    {
    public:
        static DAVConfigIndex& GetSingleton();

        void Load();
        [[nodiscard]] bool IsArmorRelevant(const WornArmorState& armor) const;

    private:
        static std::optional<FormIdentity> ParseStableForm(std::string_view value);
        static bool ArmorAddonUsesSlot(RE::TESObjectARMA* addon, std::uint32_t slotNumber);

        std::unordered_set<std::string> _sourceArmorAddons;
        std::unordered_set<std::uint32_t> _sourceSlots;
        bool _loaded{ false };
    };
}
