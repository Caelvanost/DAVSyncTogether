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

    std::uint64_t DAVStateSnapshot::StateHash() const noexcept
    {
        std::uint64_t hash = kFnvOffset;
        FeedByte(hash, davLoaded ? 1 : 0);

        for (const auto& armor : wornArmors) {
            FeedU32(hash, armor.formID);
        }

        for (std::uint32_t shift = 0; shift < 64; shift += 8) {
            FeedByte(hash, static_cast<std::uint8_t>((sceneHash >> shift) & 0xFF));
        }

        return hash;
    }

    std::string DAVStateSnapshot::Summary() const
    {
        return fmt::format(
            "player={} davLoaded={} wornArmors={} sceneNodes={} sceneHash={:016X}",
            playerName.empty() ? "<unknown>" : playerName,
            davLoaded ? 1 : 0,
            wornArmors.size(),
            sceneNodes.size(),
            sceneHash);
    }
}
