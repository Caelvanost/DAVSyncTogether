#pragma once

#include "AppearanceSnapshot.h"
#include "SkeeInterfaces.h"

namespace DAVSyncTogether
{
    class RaceMenuProbe
    {
    public:
        static RaceMenuProbe& GetSingleton();

        bool Initialize();
        [[nodiscard]] bool IsReady() const noexcept;

        AppearanceSnapshot CaptureLocalPlayer(RE::PlayerCharacter* player) const;
        void LogSnapshot(const AppearanceSnapshot& snapshot, RE::PlayerCharacter* player) const;

    private:
        class BodyMorphVisitor;

        RaceMenuProbe() = default;

        void CaptureFaceMorphs(RE::PlayerCharacter* player, AppearanceSnapshot& snapshot) const;
        void CaptureBodyMorphs(RE::PlayerCharacter* player, AppearanceSnapshot& snapshot) const;
        void CaptureHeadParts(RE::PlayerCharacter* player, AppearanceSnapshot& snapshot) const;
        void CaptureOverlayNodes(RE::NiAVObject* object, AppearanceSnapshot& snapshot) const;
        void LogOverlayInterface(RE::PlayerCharacter* player) const;

        static std::string HeadPartTypeName(RE::BGSHeadPart::HeadPartType type);
        static std::string OverlayTypeName(SKEE::IOverlayInterface::OverlayType type);
        static std::string OverlayLocationName(SKEE::IOverlayInterface::OverlayLocation location);
        static bool ContainsInsensitive(std::string_view haystack, std::string_view needle);

        SKEE::IInterfaceMap* _interfaceMap{ nullptr };
        SKEE::IBodyMorphInterface* _bodyMorph{ nullptr };
        SKEE::IOverlayInterface* _overlay{ nullptr };
    };
}
