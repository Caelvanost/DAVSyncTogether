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
            const std::vector<std::string>& candidates) const
        {
            if (candidates.empty()) {
                return std::nullopt;
            }

            int bestScore = std::numeric_limits<int>::min();
            std::optional<std::string> best;

            for (const auto& name : candidates) {
                const auto it = _variants.find(name);
                if (it == _variants.end()) {
                    continue;
                }

                const int score = ScoreHiddenVariant(armor, it->second);
                if (!best || score > bestScore || (score == bestScore && name < *best)) {
                    bestScore = score;
                    best = name;
                }
            }

            // For head-slot equipment, never select a variant that explicitly hides
            // the whole head. Prefer DAV's own showAll/showHead semantics.
            if (best && bestScore > -500) {
                return best;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::string> ChoosePreferredReplacedVariant(
            const WornArmorState& armor,
            const std::vector<std::string>& candidates) const
        {
            if (candidates.empty()) {
                return std::nullopt;
            }

            int bestScore = std::numeric_limits<int>::min();
            std::optional<std::string> best;
            bool tied = false;

            for (const auto& name : candidates) {
                const auto it = _variants.find(name);
                if (it == _variants.end()) {
                    continue;
                }

                const int score = ScoreReplacedVariant(armor, it->second);
                if (!best || score > bestScore) {
                    bestScore = score;
                    best = name;
                    tied = false;
                } else if (score == bestScore) {
                    tied = true;
                }
            }

            // REPLACED must remain conservative: if two rules are equally specific,
            // do not guess because their non-ARMA semantics (including overrideHead)
            // may differ even when they render the same replacement set.
            return best && !tied ? best : std::nullopt;
        }

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

        [[nodiscard]] int ScoreHiddenVariant(const WornArmorState& armor, const VariantRule& variant) const
        {
            int score = 0;

            if (variant.overrideHead == "showAll") {
                score += 1000;
            } else if (variant.overrideHead == "showHead") {
                score += 800;
            } else if (variant.overrideHead == "hideHair") {
                score -= 200;
            } else if (variant.overrideHead == "hideAll") {
                score -= 2000;
            }

            for (const auto& base : armor.baseArmorAddons) {
                if (variant.replaceByForm.contains(base.StableKey())) {
                    score += 2000;
                    continue;
                }

                auto* form = base.Resolve();
                auto* addon = form ? form->As<RE::TESObjectARMA>() : nullptr;
                if (!addon) {
                    continue;
                }

                if (variant.replaceBySlot.contains(30) && ArmorAddonUsesSlot(addon, 30)) {
                    score += 600;
                }
                if (variant.replaceBySlot.contains(31) && ArmorAddonUsesSlot(addon, 31)) {
                    score += 400;
                }
            }

            return score;
        }

        [[nodiscard]] int ScoreReplacedVariant(const WornArmorState& armor, const VariantRule& variant) const
        {
            int score = 0;

            for (const auto& base : armor.baseArmorAddons) {
                if (variant.replaceByForm.contains(base.StableKey())) {
                    // A form-specific rule is much stronger evidence than a generic slot rule.
                    score += 10000;
                    continue;
                }

                auto* form = base.Resolve();
                auto* addon = form ? form->As<RE::TESObjectARMA>() : nullptr;
                if (!addon) {
                    continue;
                }

                for (const auto& [slot, replacements] : variant.replaceBySlot) {
                    (void)replacements;
                    if (ArmorAddonUsesSlot(addon, slot)) {
                        score += 1000;
                        break;
                    }
                }
            }

            // Prefer the narrower rule when two candidates produce the same ARMA set.
            score -= static_cast<int>(variant.replaceByForm.size() * 10);
            score -= static_cast<int>(variant.replaceBySlot.size());
            return score;
        }

        std::unordered_set<std::string> _sourceArmorAddons;
        std::unordered_set<std::uint32_t> _sourceSlots;
        std::unordered_map<std::string, VariantRule> _variants;
        bool _loaded{ false };
    };
}
