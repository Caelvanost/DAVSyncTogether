#pragma once

#include "PCH.h"

namespace DAVSyncTogether
{
    struct MorphValue
    {
        std::string name;
        float value{ 0.0F };
    };

    struct OverlayLayer
    {
        std::string node;
        std::string texture;
        std::uint32_t color{ 0xFFFFFFFFu };
        float alpha{ 1.0F };
    };

    struct HeadPartIdentity
    {
        std::uint32_t formID{ 0 };
        std::string editorID;
        std::string type;
    };

    struct AppearanceSnapshot
    {
        static constexpr std::uint32_t kProtocolVersion = 1;

        std::uint32_t protocolVersion{ kProtocolVersion };
        std::string playerName;
        std::string raceEditorID;

        std::vector<MorphValue> faceMorphs;
        std::vector<MorphValue> bodyMorphs;
        std::vector<OverlayLayer> faceOverlays;
        std::vector<OverlayLayer> bodyOverlays;
        std::vector<HeadPartIdentity> headParts;

        std::string skinIdentity;
        std::string bodyTextureSet;

        [[nodiscard]] bool Empty() const noexcept;
        [[nodiscard]] std::string Summary() const;
    };
}
