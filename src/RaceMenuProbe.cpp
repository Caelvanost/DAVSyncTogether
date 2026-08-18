#include "RaceMenuProbe.h"

namespace DAVSyncTogether
{
    class RaceMenuProbe::BodyMorphVisitor final : public SKEE::IBodyMorphInterface::MorphValueVisitor
    {
    public:
        void Visit(
            RE::TESObjectREFR*,
            const char* morphName,
            const char* morphKey,
            float value) override
        {
            if (!morphName || !*morphName || !morphKey || !*morphKey || !std::isfinite(value)) {
                return;
            }

            values.push_back(MorphValue{
                morphName,
                morphKey,
                value });
        }

        std::vector<MorphValue> values;
    };

    RaceMenuProbe& RaceMenuProbe::GetSingleton()
    {
        static RaceMenuProbe singleton;
        return singleton;
    }

    bool RaceMenuProbe::Initialize()
    {
        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) {
            SKSE::log::critical("DAVST SKEE init failed: no SKSE messaging interface");
            return false;
        }

        SKEE::InterfaceExchangeMessage exchange{};
        const bool dispatched = messaging->Dispatch(
            SKEE::InterfaceExchangeMessage::kMessageExchangeInterface,
            &exchange,
            sizeof(exchange),
            "skee");

        if (!dispatched || !exchange.interfaceMap) {
            SKSE::log::critical(
                "DAVST SKEE interface exchange failed dispatched={} map={}",
                dispatched ? 1 : 0,
                exchange.interfaceMap ? 1 : 0);
            return false;
        }

        _interfaceMap = exchange.interfaceMap;

        if (auto* base = _interfaceMap->QueryInterface("BodyMorph")) {
            _bodyMorph = static_cast<SKEE::IBodyMorphInterface*>(base);
        }
        if (auto* base = _interfaceMap->QueryInterface("Overlay")) {
            _overlay = static_cast<SKEE::IOverlayInterface*>(base);
        }

        SKSE::log::info(
            "DAVST SKEE interfaces READY bodyMorph={} bodyMorphVersion={} overlay={} overlayVersion={}",
            _bodyMorph ? 1 : 0,
            _bodyMorph ? _bodyMorph->GetVersion() : 0,
            _overlay ? 1 : 0,
            _overlay ? _overlay->GetVersion() : 0);

        return _bodyMorph != nullptr;
    }

    bool RaceMenuProbe::IsReady() const noexcept
    {
        return _bodyMorph != nullptr;
    }

    AppearanceSnapshot RaceMenuProbe::CaptureLocalPlayer(RE::PlayerCharacter* player) const
    {
        AppearanceSnapshot snapshot;
        if (!player) {
            return snapshot;
        }

        if (const char* name = player->GetName(); name) {
            snapshot.playerName = name;
        }

        if (auto* race = player->GetRace()) {
            if (const char* editorID = race->GetFormEditorID(); editorID) {
                snapshot.raceEditorID = editorID;
            }
        }

        CaptureFaceMorphs(player, snapshot);
        CaptureBodyMorphs(player, snapshot);
        CaptureHeadParts(player, snapshot);

        if (auto* skin = player->GetSkin()) {
            if (const char* editorID = skin->GetFormEditorID(); editorID) {
                snapshot.skinIdentity = editorID;
            }
        }

        if (auto* root = player->Get3D()) {
            CaptureOverlayNodes(root, snapshot);
        }

        return snapshot;
    }

    void RaceMenuProbe::CaptureFaceMorphs(
        RE::PlayerCharacter* player,
        AppearanceSnapshot& snapshot) const
    {
        auto* npc = player ? player->GetActorBase() : nullptr;
        if (!npc || !npc->faceData) {
            return;
        }

        for (std::uint32_t i = 0; i < RE::TESNPC::FaceData::Morphs::kTotal; ++i) {
            const float value = npc->faceData->morphs[i];
            if (!std::isfinite(value)) {
                continue;
            }

            snapshot.faceMorphs.push_back(MorphValue{
                fmt::format("FaceMorph{:02}", i),
                "TESNPC",
                value });
        }
    }

    void RaceMenuProbe::CaptureBodyMorphs(
        RE::PlayerCharacter* player,
        AppearanceSnapshot& snapshot) const
    {
        if (!player || !_bodyMorph) {
            return;
        }

        BodyMorphVisitor visitor;
        _bodyMorph->VisitMorphValues(player, visitor);

        std::sort(visitor.values.begin(), visitor.values.end(), [](const MorphValue& lhs, const MorphValue& rhs) {
            if (lhs.name != rhs.name) {
                return lhs.name < rhs.name;
            }
            if (lhs.key != rhs.key) {
                return lhs.key < rhs.key;
            }
            return lhs.value < rhs.value;
        });

        snapshot.bodyMorphs = std::move(visitor.values);
    }

    void RaceMenuProbe::CaptureHeadParts(
        RE::PlayerCharacter* player,
        AppearanceSnapshot& snapshot) const
    {
        auto* npc = player ? player->GetActorBase() : nullptr;
        if (!npc) {
            return;
        }

        for (std::uint32_t raw = 0;
             raw < static_cast<std::uint32_t>(RE::BGSHeadPart::HeadPartType::kTotal);
             ++raw) {
            const auto type = static_cast<RE::BGSHeadPart::HeadPartType>(raw);
            auto* part = npc->GetCurrentHeadPartByType(type);
            if (!part) {
                continue;
            }

            HeadPartIdentity identity;
            identity.formID = part->GetFormID();
            if (const char* editorID = part->GetFormEditorID(); editorID) {
                identity.editorID = editorID;
            }
            identity.type = HeadPartTypeName(type);
            snapshot.headParts.push_back(std::move(identity));
        }
    }

    void RaceMenuProbe::CaptureOverlayNodes(
        RE::NiAVObject* object,
        AppearanceSnapshot& snapshot) const
    {
        if (!object) {
            return;
        }

        const char* rawName = object->name.c_str();
        const std::string_view name = rawName ? std::string_view(rawName) : std::string_view{};

        if (name.find("[Ovl") != std::string_view::npos ||
            name.find("[SOvl") != std::string_view::npos) {
            OverlayLayer layer;
            layer.node = std::string(name);

            if (ContainsInsensitive(name, "face")) {
                snapshot.faceOverlays.push_back(std::move(layer));
            } else {
                snapshot.bodyOverlays.push_back(std::move(layer));
            }
        }

        if (auto* node = object->AsNode()) {
            for (auto& child : node->GetChildren()) {
                if (child) {
                    CaptureOverlayNodes(child.get(), snapshot);
                }
            }
        }
    }

    void RaceMenuProbe::LogOverlayInterface(RE::PlayerCharacter* player) const
    {
        if (!_overlay || !player) {
            SKSE::log::info("DAVST OVERLAY interface unavailable");
            return;
        }

        SKSE::log::info(
            "DAVST OVERLAY actorHasOverlays={}",
            _overlay->HasOverlays(player) ? 1 : 0);

        for (const auto type : {
                 SKEE::IOverlayInterface::OverlayType::Normal,
                 SKEE::IOverlayInterface::OverlayType::Spell }) {
            for (const auto location : {
                     SKEE::IOverlayInterface::OverlayLocation::Body,
                     SKEE::IOverlayInterface::OverlayLocation::Hand,
                     SKEE::IOverlayInterface::OverlayLocation::Feet,
                     SKEE::IOverlayInterface::OverlayLocation::Face }) {
                const auto count = _overlay->GetOverlayCount(type, location);
                const char* format = _overlay->GetOverlayFormat(type, location);
                SKSE::log::info(
                    "DAVST OVERLAY slots type={} location={} count={} format=\"{}\"",
                    OverlayTypeName(type),
                    OverlayLocationName(location),
                    count,
                    format ? format : "");
            }
        }
    }

    void RaceMenuProbe::LogSnapshot(
        const AppearanceSnapshot& snapshot,
        RE::PlayerCharacter* player) const
    {
        SKSE::log::info("DAVST APPEARANCE SNAPSHOT {}", snapshot.Summary());

        if (_bodyMorph && player) {
            SKSE::log::info(
                "DAVST BODY_MORPH state hasMorphs={} evaluated={}",
                _bodyMorph->HasMorphs(player) ? 1 : 0,
                _bodyMorph->EvaluateBodyMorphs(player));
        }

        for (const auto& morph : snapshot.faceMorphs) {
            SKSE::log::info(
                "DAVST FACE_MORPH name=\"{}\" key=\"{}\" value={:.9g}",
                morph.name,
                morph.key,
                morph.value);
        }

        for (const auto& morph : snapshot.bodyMorphs) {
            SKSE::log::info(
                "DAVST BODY_MORPH name=\"{}\" key=\"{}\" value={:.9g}",
                morph.name,
                morph.key,
                morph.value);
        }

        for (const auto& part : snapshot.headParts) {
            SKSE::log::info(
                "DAVST HEAD_PART type={} form={:08X} editorID=\"{}\"",
                part.type,
                part.formID,
                part.editorID);
        }

        for (const auto& overlay : snapshot.faceOverlays) {
            SKSE::log::info("DAVST FACE_OVERLAY node=\"{}\"", overlay.node);
        }
        for (const auto& overlay : snapshot.bodyOverlays) {
            SKSE::log::info("DAVST BODY_OVERLAY node=\"{}\"", overlay.node);
        }

        if (!snapshot.skinIdentity.empty()) {
            SKSE::log::info("DAVST SKIN editorID=\"{}\"", snapshot.skinIdentity);
        }

        LogOverlayInterface(player);
    }

    std::string RaceMenuProbe::HeadPartTypeName(RE::BGSHeadPart::HeadPartType type)
    {
        switch (type) {
        case RE::BGSHeadPart::HeadPartType::kMisc:
            return "Misc";
        case RE::BGSHeadPart::HeadPartType::kFace:
            return "Face";
        case RE::BGSHeadPart::HeadPartType::kEyes:
            return "Eyes";
        case RE::BGSHeadPart::HeadPartType::kHair:
            return "Hair";
        case RE::BGSHeadPart::HeadPartType::kFacialHair:
            return "FacialHair";
        case RE::BGSHeadPart::HeadPartType::kScar:
            return "Scar";
        case RE::BGSHeadPart::HeadPartType::kEyebrows:
            return "Eyebrows";
        default:
            return "Unknown";
        }
    }

    std::string RaceMenuProbe::OverlayTypeName(SKEE::IOverlayInterface::OverlayType type)
    {
        return type == SKEE::IOverlayInterface::OverlayType::Normal ? "Normal" : "Spell";
    }

    std::string RaceMenuProbe::OverlayLocationName(SKEE::IOverlayInterface::OverlayLocation location)
    {
        switch (location) {
        case SKEE::IOverlayInterface::OverlayLocation::Body:
            return "Body";
        case SKEE::IOverlayInterface::OverlayLocation::Hand:
            return "Hand";
        case SKEE::IOverlayInterface::OverlayLocation::Feet:
            return "Feet";
        case SKEE::IOverlayInterface::OverlayLocation::Face:
            return "Face";
        default:
            return "Unknown";
        }
    }

    bool RaceMenuProbe::ContainsInsensitive(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty()) {
            return true;
        }

        return std::search(
                   haystack.begin(),
                   haystack.end(),
                   needle.begin(),
                   needle.end(),
                   [](char lhs, char rhs) {
                       return std::tolower(static_cast<unsigned char>(lhs)) ==
                              std::tolower(static_cast<unsigned char>(rhs));
                   }) != haystack.end();
    }
}
