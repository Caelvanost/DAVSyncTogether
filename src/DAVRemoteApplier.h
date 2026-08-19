#pragma once

#include "NetworkProtocol.h"

namespace DAVSyncTogether
{
    struct RemoteApplyResult
    {
        bool supported{ false };
        std::size_t matchedNodes{ 0 };
        std::size_t changedNodes{ 0 };
        std::size_t headFixes{ 0 };
    };

    class DAVRemoteApplier
    {
    public:
        static RemoteApplyResult Apply(RE::Actor* proxyActor, const RemoteArmorState& state);

    private:
        static void VisitAndApply(
            RE::NiAVObject* object,
            RE::FormID armorFormID,
            bool cull,
            RemoteApplyResult& result);
        static void ApplyHeadHairVisibility(
            RE::Actor* actor,
            RE::TESObjectARMO* armor,
            bool hidden,
            RemoteApplyResult& result);

        static std::optional<std::pair<RE::FormID, RE::FormID>> ParseArmorNode(std::string_view name);
        static std::optional<RE::FormID> ParseFormID(std::string_view text);
    };
}
