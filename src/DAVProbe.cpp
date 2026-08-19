#include "DAVProbe.h"

#include <Windows.h>

namespace DAVSyncTogether
{
    namespace
    {
        constexpr auto kProbeInterval = std::chrono::milliseconds(500);
        constexpr auto kSleepSlice = std::chrono::milliseconds(100);
    }

    DAVProbe& DAVProbe::GetSingleton()
    {
        static DAVProbe singleton;
        return singleton;
    }

    DAVProbe::~DAVProbe()
    {
        Stop();
    }

    void DAVProbe::Start()
    {
        if (_running.exchange(true)) {
            return;
        }

        SKSE::log::info(
            "DAVST DAV armor-state probe started interval={}ms davModuleLoaded={}",
            kProbeInterval.count(),
            IsDAVLoaded() ? 1 : 0);

        _thread = std::jthread([this](std::stop_token token) {
            while (!token.stop_requested() && _running.load()) {
                QueueProbe();

                auto slept = std::chrono::milliseconds(0);
                while (slept < kProbeInterval && !token.stop_requested() && _running.load()) {
                    std::this_thread::sleep_for(kSleepSlice);
                    slept += kSleepSlice;
                }
            }
        });
    }

    void DAVProbe::Stop()
    {
        if (!_running.exchange(false)) {
            return;
        }

        if (_thread.joinable()) {
            _thread.request_stop();
            _thread.join();
        }
        _tickQueued.store(false);
    }

    void DAVProbe::Reset()
    {
        _hasPrevious = false;
        _previous = {};
        SKSE::log::info("DAVST DAV armor-state probe reset");
    }

    void DAVProbe::QueueProbe(std::string reason)
    {
        if (!_running.load() || _tickQueued.exchange(true)) {
            return;
        }

        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            _tickQueued.store(false);
            return;
        }

        tasks->AddTask([this, reason = std::move(reason)]() mutable {
            _tickQueued.store(false);
            TickOnGameThread(std::move(reason));
        });
    }

    DAVStateSnapshot DAVProbe::CaptureLocalPlayer(RE::PlayerCharacter* player) const
    {
        DAVStateSnapshot snapshot;
        snapshot.davLoaded = IsDAVLoaded();

        if (!player || !player->Get3D()) {
            return snapshot;
        }

        if (const char* name = player->GetName(); name) {
            snapshot.playerName = name;
        }

        ActiveAddonMap activeAddons;
        VisitArmorNodes(player->Get3D(), activeAddons);
        for (auto& [armo, addons] : activeAddons) {
            std::sort(addons.begin(), addons.end());
            addons.erase(std::unique(addons.begin(), addons.end()), addons.end());
        }

        const auto inventory = player->GetInventory([](RE::TESBoundObject& object) {
            return object.IsArmor();
        });

        for (const auto& [item, data] : inventory) {
            const auto& [count, entry] = data;
            if (count <= 0 || !entry || !entry->IsWorn()) {
                continue;
            }

            auto* armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
            if (!armor || armor->armorAddons.empty()) {
                continue;
            }

            WornArmorState state;
            state.formID = armor->GetFormID();
            if (const char* editorID = armor->GetFormEditorID(); editorID) {
                state.editorID = editorID;
            }
            if (const char* name = armor->GetName(); name) {
                state.name = name;
            }

            for (auto* addon : armor->armorAddons) {
                if (addon) {
                    state.baseArmorAddons.push_back(addon->GetFormID());
                }
            }
            std::sort(state.baseArmorAddons.begin(), state.baseArmorAddons.end());
            state.baseArmorAddons.erase(
                std::unique(state.baseArmorAddons.begin(), state.baseArmorAddons.end()),
                state.baseArmorAddons.end());

            if (const auto it = activeAddons.find(state.formID); it != activeAddons.end()) {
                state.activeArmorAddons = it->second;
            }

            state.visualState = ClassifyVisualState(
                state.baseArmorAddons,
                state.activeArmorAddons);

            snapshot.wornArmors.push_back(std::move(state));
        }

        std::sort(snapshot.wornArmors.begin(), snapshot.wornArmors.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.formID < rhs.formID;
        });

        return snapshot;
    }

    void DAVProbe::TickOnGameThread(std::string reason)
    {
        if (!_running.load()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Get3D()) {
            return;
        }

        auto current = CaptureLocalPlayer(player);
        if (current.playerName.empty() || current.playerName == "Prisoner") {
            return;
        }

        if (!_hasPrevious || current.StateHash() != _previous.StateHash()) {
            LogSnapshotDiff(current, reason);
            _previous = std::move(current);
            _hasPrevious = true;
        }
    }

    void DAVProbe::LogSnapshotDiff(const DAVStateSnapshot& current, std::string_view reason)
    {
        SKSE::log::info(
            "DAVST DAV_STATE reason={} {} stateHash={:016X}",
            reason,
            current.Summary(),
            current.StateHash());

        std::unordered_map<RE::FormID, const WornArmorState*> previousByForm;
        if (_hasPrevious) {
            for (const auto& armor : _previous.wornArmors) {
                previousByForm.emplace(armor.formID, std::addressof(armor));
            }
        }

        for (const auto& armor : current.wornArmors) {
            bool changed = !_hasPrevious;
            if (_hasPrevious) {
                const auto it = previousByForm.find(armor.formID);
                changed = it == previousByForm.end() || !armor.VisualEquivalent(*it->second);
            }

            if (!changed) {
                continue;
            }

            SKSE::log::info(
                "DAVST ARMOR_STATE armo={:08X} editorID=\"{}\" name=\"{}\" state={} baseARMA={} activeARMA={}",
                armor.formID,
                armor.editorID,
                armor.name,
                ArmorVisualStateName(armor.visualState),
                FormatFormIDs(armor.baseArmorAddons),
                FormatFormIDs(armor.activeArmorAddons));
        }

        if (_hasPrevious) {
            std::unordered_set<RE::FormID> currentForms;
            for (const auto& armor : current.wornArmors) {
                currentForms.insert(armor.formID);
            }

            for (const auto& armor : _previous.wornArmors) {
                if (!currentForms.contains(armor.formID)) {
                    SKSE::log::info(
                        "DAVST ARMOR_STATE armo={:08X} editorID=\"{}\" name=\"{}\" state=UNEQUIPPED baseARMA={} activeARMA=[]",
                        armor.formID,
                        armor.editorID,
                        armor.name,
                        FormatFormIDs(armor.baseArmorAddons));
                }
            }
        }
    }

    bool DAVProbe::IsDAVLoaded()
    {
        return ::GetModuleHandleW(L"DynamicArmorVariants.dll") != nullptr;
    }

    void DAVProbe::VisitArmorNodes(RE::NiAVObject* object, ActiveAddonMap& activeAddons)
    {
        if (!object) {
            return;
        }

        const char* rawName = object->name.c_str();
        if (rawName && *rawName) {
            if (const auto parsed = ParseArmorNode(rawName)) {
                const auto [arma, armo] = *parsed;
                activeAddons[armo].push_back(arma);
            }
        }

        if (auto* node = object->AsNode()) {
            for (auto& child : node->GetChildren()) {
                if (child) {
                    VisitArmorNodes(child.get(), activeAddons);
                }
            }
        }
    }

    std::optional<std::pair<RE::FormID, RE::FormID>> DAVProbe::ParseArmorNode(std::string_view name)
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

        // Skyrim biped armor geometry names put an ARMA before the slash and
        // the owning ARMO after it, e.g. "(FE02380B)[0]/ (FE023803) [100%]".
        return std::pair{ *arma, *armo };
    }

    std::optional<RE::FormID> DAVProbe::ParseFormID(std::string_view text)
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

    ArmorVisualState DAVProbe::ClassifyVisualState(
        const std::vector<RE::FormID>& baseAddons,
        const std::vector<RE::FormID>& activeAddons) noexcept
    {
        if (baseAddons.empty()) {
            return ArmorVisualState::Unknown;
        }
        if (activeAddons.empty()) {
            return ArmorVisualState::Hidden;
        }

        const bool allBase = std::all_of(activeAddons.begin(), activeAddons.end(), [&](RE::FormID active) {
            return std::binary_search(baseAddons.begin(), baseAddons.end(), active);
        });

        return allBase ? ArmorVisualState::Visible : ArmorVisualState::Replaced;
    }

    std::string DAVProbe::FormatFormIDs(const std::vector<RE::FormID>& values)
    {
        std::string result = "[";
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i != 0) {
                result += ',';
            }
            result += fmt::format("{:08X}", values[i]);
        }
        result += ']';
        return result;
    }
}
