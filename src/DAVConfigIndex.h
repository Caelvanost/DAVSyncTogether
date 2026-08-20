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
        [[nodiscard]] std::vector<std::string> FindMatchingVariants(const WornArmorState& armor) const;
        [[nodiscard]] std::optional<std::string> ChoosePreferredHiddenVariant(
            const WornArmorState& armor,
            const std::vector<std::string>& candidates) const;

    private:
        struct VariantRule
        {
            std::string name;
            std::string linkTo;
            std::string overrideHead;
            std::unordered_map<std::string, std::vector<FormIdentity>> replaceByForm;
            std::unordered_map<std::uint32_t, std::vector<FormIdentity>> replaceBySlot;
        };

        static std::optional<FormIdentity> ParseStableForm(std::string_view value);
        static bool ArmorAddonUsesSlot(RE::TESObjectARMA* addon, std::uint32_t slotNumber);
        static void SortAndUnique(std::vector<FormIdentity>& values);

        [[nodiscard]] std::vector<FormIdentity> BuildExpectedActive(
            const WornArmorState& armor,
            const VariantRule& variant,
            bool& affected) const;
        [[nodiscard]] int ScoreHiddenVariant(const WornArmorState& armor, const VariantRule& variant) const;

        std::unordered_set<std::string> _sourceArmorAddons;
        std::unordered_set<std::uint32_t> _sourceSlots;
        std::unordered_map<std::string, VariantRule> _variants;
        bool _loaded{ false };
    };
}
