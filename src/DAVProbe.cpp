#include "DAVProbe.h"

#include <Windows.h>

namespace DAVSyncTogether
{
    namespace
    {
        constexpr auto kProbeInterval = std::chrono::milliseconds(500);
        constexpr auto kSleepSlice = std::chrono::milliseconds(100);
        constexpr std::size_t kMaxSceneNodes = 2048;
        constexpr std::size_t kMaxDiffLog = 128;
        constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
        constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
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
            "DAVST DAV probe started interval={}ms davModuleLoaded={}",
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
        SKSE::log::info("DAVST DAV probe state reset");
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

        if (!player) {
            return snapshot;
        }

        if (const char* name = player->GetName(); name) {
            snapshot.playerName = name;
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
            if (!armor) {
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
            snapshot.wornArmors.push_back(std::move(state));
        }

        std::sort(snapshot.wornArmors.begin(), snapshot.wornArmors.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.formID < rhs.formID;
        });

        if (auto* root = player->Get3D()) {
            VisitScene(root, snapshot.sceneNodes);
            std::sort(snapshot.sceneNodes.begin(), snapshot.sceneNodes.end());
            snapshot.sceneNodes.erase(
                std::unique(snapshot.sceneNodes.begin(), snapshot.sceneNodes.end()),
                snapshot.sceneNodes.end());
            snapshot.sceneHash = HashStrings(snapshot.sceneNodes);
        }

        return snapshot;
    }

    void DAVProbe::TickOnGameThread(std::string reason)
    {
        if (!_running.load()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        auto current = CaptureLocalPlayer(player);
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

        for (const auto& armor : current.wornArmors) {
            SKSE::log::info(
                "DAVST WORN_ARMOR form={:08X} editorID=\"{}\" name=\"{}\"",
                armor.formID,
                armor.editorID,
                armor.name);
        }

        if (!_hasPrevious) {
            SKSE::log::info("DAVST SCENE baseline captured nodes={}", current.sceneNodes.size());
            return;
        }

        std::vector<std::string> added;
        std::vector<std::string> removed;
        std::set_difference(
            current.sceneNodes.begin(), current.sceneNodes.end(),
            _previous.sceneNodes.begin(), _previous.sceneNodes.end(),
            std::back_inserter(added));
        std::set_difference(
            _previous.sceneNodes.begin(), _previous.sceneNodes.end(),
            current.sceneNodes.begin(), current.sceneNodes.end(),
            std::back_inserter(removed));

        SKSE::log::info(
            "DAVST SCENE_DIFF added={} removed={}",
            added.size(),
            removed.size());

        for (std::size_t i = 0; i < std::min(added.size(), kMaxDiffLog); ++i) {
            SKSE::log::info("DAVST SCENE_NODE + \"{}\"", added[i]);
        }
        for (std::size_t i = 0; i < std::min(removed.size(), kMaxDiffLog); ++i) {
            SKSE::log::info("DAVST SCENE_NODE - \"{}\"", removed[i]);
        }

        if (added.size() > kMaxDiffLog || removed.size() > kMaxDiffLog) {
            SKSE::log::info("DAVST SCENE_DIFF log truncated at {} nodes per direction", kMaxDiffLog);
        }
    }

    bool DAVProbe::IsDAVLoaded()
    {
        return ::GetModuleHandleW(L"DynamicArmorVariants.dll") != nullptr;
    }

    void DAVProbe::VisitScene(RE::NiAVObject* object, std::vector<std::string>& names)
    {
        if (!object || names.size() >= kMaxSceneNodes) {
            return;
        }

        const char* rawName = object->name.c_str();
        if (rawName && *rawName) {
            names.emplace_back(rawName);
        }

        if (auto* node = object->AsNode()) {
            for (auto& child : node->GetChildren()) {
                if (child) {
                    VisitScene(child.get(), names);
                }
                if (names.size() >= kMaxSceneNodes) {
                    break;
                }
            }
        }
    }

    std::uint64_t DAVProbe::HashStrings(const std::vector<std::string>& values) noexcept
    {
        std::uint64_t hash = kFnvOffset;
        for (const auto& value : values) {
            for (const unsigned char ch : value) {
                hash ^= ch;
                hash *= kFnvPrime;
            }
            hash ^= 0xFF;
            hash *= kFnvPrime;
        }
        return hash;
    }
}
