#include "DAVStateSnapshot.h"

namespace DAVSyncTogether
{
    namespace
    {
        constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
        constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

        void FeedByte(std::uint64_t& hash, std::uint8_t value) noexcept
        {
            hash ^= value;
            hash *= kFnvPrime;
        }

        void FeedU32(std::uint64_t& hash, std::uint32_t value) noexcept
        {
            for (std::uint32_t shift = 0; shift < 32; shift += 8) {
                FeedByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xFF));
            }
        }
    }

    bool WornArmorState::VisualEquivalent(const WornArmorState& rhs) const noexcept
    {
        return formID == rhs.formID &&
               visualState == rhs.visualState &&
               activeArmorAddons == rhs.activeArmorAddons;
    }

    std::uint64_t DAVStateSnapshot::StateHash() const noexcept
    {
        std::uint64_t hash = kFnvOffset;
        FeedByte(hash, davLoaded ? 1 : 0);

        for (const auto& armor : wornArmors) {
            FeedU32(hash, armor.formID);
            FeedByte(hash, static_cast<std::uint8_t>(armor.visualState));
            for (const auto addon : armor.activeArmorAddons) {
                FeedU32(hash, addon);
            }
            FeedByte(hash, 0xFF);
        }

        return hash;
    }

    std::string DAVStateSnapshot::Summary() const
    {
        return fmt::format(
            "player={} davLoaded={} wornArmors={}",
            playerName.empty() ? "<unknown>" : playerName,
            davLoaded ? 1 : 0,
            wornArmors.size());
    }

    std::string_view ArmorVisualStateName(ArmorVisualState state) noexcept
    {
        switch (state) {
        case ArmorVisualState::Visible:
            return "VISIBLE";
        case ArmorVisualState::Hidden:
            return "HIDDEN";
        case ArmorVisualState::Replaced:
            return "REPLACED";
        default:
            return "UNKNOWN";
        }
    }
}
