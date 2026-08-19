#include "DAVRemoteApplier.h"

namespace DAVSyncTogether
{
    RemoteApplyResult DAVRemoteApplier::Apply(RE::Actor* proxyActor, const RemoteArmorState& state)
    {
        RemoteApplyResult result;
        if (!proxyActor || !proxyActor->Get3D() || state.armor.runtimeFormID == 0) {
            return result;
        }

        bool cull = false;
        switch (state.state) {
        case NetworkArmorState::Hidden:
            result.supported = true;
            cull = true;
            break;
        case NetworkArmorState::Visible:
        case NetworkArmorState::Unequipped:
            result.supported = true;
            cull = false;
            break;
        case NetworkArmorState::Replaced:
        default:
            return result;
        }

        VisitAndApply(proxyActor->Get3D(), state.armor.runtimeFormID, cull, result);

        if (auto* form = RE::TESForm::LookupByID(state.armor.runtimeFormID)) {
            if (auto* armor = form->As<RE::TESObjectARMO>()) {
                ApplyHeadHairVisibility(
                    proxyActor,
                    armor,
                    state.state == NetworkArmorState::Hidden,
                    result);
            }
        }

        return result;
    }

    void DAVRemoteApplier::ApplyHeadHairVisibility(
        RE::Actor* actor,
        RE::TESObjectARMO* armor,
        bool hidden,
        RemoteApplyResult& result)
    {
        if (!actor || !armor) {
            return;
        }

        const auto headSlot = static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1u << 0);
        const auto hairSlot = static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1u << 1);
        const bool usesHead = armor->HasPartOf(headSlot);
        const bool usesHair = armor->HasPartOf(hairSlot);

        if (!usesHead && !usesHair) {
            return;
        }

        // DAV hidden-helmet variants normally use overrideHead=showAll. The proxy still
        // has the original ARMO equipped, so Skyrim continues to hide face/hair unless
        // DAVSync restores those head parts explicitly.
        if (usesHead) {
            if (auto* face = actor->GetFaceNodeSkinned()) {
                const bool wantCull = !hidden;
                if (face->GetAppCulled() != wantCull) {
                    face->CullNode(wantCull);
                    ++result.headFixes;
                }
            }
        }

        if (usesHair) {
            if (auto* hair = actor->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::Hair)) {
                const bool wantCull = !hidden;
                if (hair->GetAppCulled() != wantCull) {
                    hair->CullNode(wantCull);
                    ++result.headFixes;
                }
            }
        }
    }

    void DAVRemoteApplier::VisitAndApply(
        RE::NiAVObject* object,
        RE::FormID armorFormID,
        bool cull,
        RemoteApplyResult& result)
    {
        if (!object) {
            return;
        }

        const char* rawName = object->name.c_str();
        if (rawName && *rawName) {
            if (const auto parsed = ParseArmorNode(rawName)) {
                const auto [arma, armo] = *parsed;
                (void)arma;
                if (armo == armorFormID) {
                    ++result.matchedNodes;
                    if (object->GetAppCulled() != cull) {
                        object->CullNode(cull);
                        ++result.changedNodes;
                    }
                }
            }
        }

        if (auto* node = object->AsNode()) {
            for (auto& child : node->GetChildren()) {
                if (child) {
                    VisitAndApply(child.get(), armorFormID, cull, result);
                }
            }
        }
    }

    std::optional<std::pair<RE::FormID, RE::FormID>> DAVRemoteApplier::ParseArmorNode(std::string_view name)
    {
        const auto firstOpen = name.find('(');
        if (firstOpen == std::string_view::npos) {
            return std::nullopt;
        }
        const auto firstClose = name.find(')', firstOpen + 1);
        if (firstClose == std::string_view::npos) {
            return std::nullopt;
        }

        const auto slash = name.find('/', firstClose + 1);
        if (slash == std::string_view::npos) {
            return std::nullopt;
        }
        const auto secondOpen = name.find('(', slash + 1);
        if (secondOpen == std::string_view::npos) {
            return std::nullopt;
        }
        const auto secondClose = name.find(')', secondOpen + 1);
        if (secondClose == std::string_view::npos) {
            return std::nullopt;
        }

        const auto arma = ParseFormID(name.substr(firstOpen + 1, firstClose - firstOpen - 1));
        const auto armo = ParseFormID(name.substr(secondOpen + 1, secondClose - secondOpen - 1));
        if (!arma || !armo) {
            return std::nullopt;
        }

        return std::pair{ *arma, *armo };
    }

    std::optional<RE::FormID> DAVRemoteApplier::ParseFormID(std::string_view text)
    {
        if (text.empty() || text.size() > 8) {
            return std::nullopt;
        }

        try {
            std::size_t consumed = 0;
            const auto value = std::stoul(std::string(text), std::addressof(consumed), 16);
            if (consumed != text.size() || value > std::numeric_limits<std::uint32_t>::max()) {
                return std::nullopt;
            }
            return static_cast<RE::FormID>(value);
        } catch (...) {
            return std::nullopt;
        }
    }
}
