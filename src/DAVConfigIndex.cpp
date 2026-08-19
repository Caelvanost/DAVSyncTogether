#include "DAVConfigIndex.h"

#include <fstream>
#include <regex>

namespace DAVSyncTogether
{
    namespace
    {
        constexpr auto kConfigRoot = "Data/SKSE/Plugins/DynamicArmorVariants";

        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }
    }

    DAVConfigIndex& DAVConfigIndex::GetSingleton()
    {
        static DAVConfigIndex singleton;
        return singleton;
    }

    void DAVConfigIndex::Load()
    {
        _sourceArmorAddons.clear();
        _sourceSlots.clear();
        _loaded = true;

        const std::filesystem::path root(kConfigRoot);
        if (!std::filesystem::exists(root)) {
            SKSE::log::warn("DAVST CONFIG_INDEX root missing path=\"{}\"", root.string());
            return;
        }

        const std::regex formKeyPattern(
            R"DAV("([\w\-. ]+\.es[lmp]\|(?:0[xX])?[0-9A-Fa-f]{1,8})"\s*:)DAV",
            std::regex::ECMAScript | std::regex::icase);
        const std::regex slotKeyPattern(
            R"DAV("(3[0-9]|4[0-9]|5[0-9]|6[0-1])"\s*:)DAV",
            std::regex::ECMAScript);

        std::size_t files = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || ToLower(entry.path().extension().string()) != ".json") {
                continue;
            }

            std::ifstream stream(entry.path(), std::ios::binary);
            if (!stream) {
                continue;
            }

            const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            ++files;

            for (std::sregex_iterator it(text.begin(), text.end(), formKeyPattern), end; it != end; ++it) {
                if (const auto identity = ParseStableForm((*it)[1].str()); identity && identity->IsStable()) {
                    _sourceArmorAddons.insert(identity->StableKey());
                }
            }

            for (std::sregex_iterator it(text.begin(), text.end(), slotKeyPattern), end; it != end; ++it) {
                try {
                    _sourceSlots.insert(static_cast<std::uint32_t>(std::stoul((*it)[1].str())));
                } catch (...) {
                }
            }
        }

        SKSE::log::info(
            "DAVST CONFIG_INDEX loaded files={} sourceARMA={} sourceSlots={}",
            files,
            _sourceArmorAddons.size(),
            _sourceSlots.size());
    }

    bool DAVConfigIndex::IsArmorRelevant(const WornArmorState& armor) const
    {
        if (!_loaded) {
            return false;
        }

        for (const auto& addonIdentity : armor.baseArmorAddons) {
            if (_sourceArmorAddons.contains(addonIdentity.StableKey())) {
                return true;
            }

            auto* form = addonIdentity.Resolve();
            auto* addon = form ? form->As<RE::TESObjectARMA>() : nullptr;
            if (!addon) {
                continue;
            }

            for (const auto slot : _sourceSlots) {
                if (ArmorAddonUsesSlot(addon, slot)) {
                    return true;
                }
            }
        }

        return false;
    }

    std::optional<FormIdentity> DAVConfigIndex::ParseStableForm(std::string_view value)
    {
        const auto separator = value.rfind('|');
        if (separator == std::string_view::npos) {
            return std::nullopt;
        }

        std::string plugin(value.substr(0, separator));
        std::string idText(value.substr(separator + 1));
        if (idText.starts_with("0x") || idText.starts_with("0X")) {
            idText.erase(0, 2);
        }

        try {
            std::size_t consumed = 0;
            const auto local = std::stoul(idText, std::addressof(consumed), 16);
            if (consumed != idText.size()) {
                return std::nullopt;
            }

            FormIdentity identity;
            identity.plugin = std::move(plugin);
            identity.localFormID = static_cast<RE::FormID>(local);
            if (auto* resolved = identity.Resolve()) {
                return MakeFormIdentity(resolved);
            }
            return identity;
        } catch (...) {
            return std::nullopt;
        }
    }

    bool DAVConfigIndex::ArmorAddonUsesSlot(RE::TESObjectARMA* addon, std::uint32_t slotNumber)
    {
        if (!addon || slotNumber < 30 || slotNumber > 61) {
            return false;
        }

        const auto mask = static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1u << (slotNumber - 30));
        return addon->HasPartOf(mask);
    }
}
