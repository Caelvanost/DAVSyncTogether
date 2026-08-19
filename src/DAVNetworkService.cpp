#include "DAVNetworkService.h"

#include "Config.h"
#include "UdpTransport.h"

namespace DAVSyncTogether
{
    namespace
    {
        std::string FormatStableIdentities(const std::vector<FormIdentity>& values)
        {
            std::string result = "[";
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0) {
                    result += ',';
                }
                result += '"';
                result += values[i].StableKey();
                result += '"';
            }
            result += ']';
            return result;
        }

        std::string FormatRuntimeIDs(const std::vector<FormIdentity>& values)
        {
            std::string result = "[";
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0) {
                    result += ',';
                }
                result += fmt::format("{:08X}", values[i].runtimeFormID);
            }
            result += ']';
            return result;
        }
    }

    DAVNetworkService& DAVNetworkService::GetSingleton()
    {
        static DAVNetworkService singleton;
        return singleton;
    }

    DAVNetworkService::~DAVNetworkService()
    {
        Stop();
    }

    bool DAVNetworkService::Start()
    {
        if (_running.load()) {
            return true;
        }

        const auto config = Config::Load();
        if (!config.networkEnabled) {
            SKSE::log::info("DAVSTNET disabled by configuration");
            return false;
        }

        const bool started = UdpTransport::GetSingleton().Start(
            config,
            [this](std::string packet) {
                QueueReceivedPacket(std::move(packet));
            });

        _running.store(started);
        if (started) {
            SKSE::log::info(
                "DAVSTNET validation mode active: RX resolves forms only; apply=0");
        }
        return started;
    }

    void DAVNetworkService::Stop()
    {
        if (!_running.exchange(false)) {
            return;
        }
        UdpTransport::GetSingleton().Stop();
    }

    void DAVNetworkService::SendArmorState(
        const WornArmorState& armor,
        bool unequipped)
    {
        if (!_running.load() || !armor.armor.IsStable()) {
            return;
        }

        const auto payload = EncodeArmorState(armor, unequipped);
        UdpTransport::GetSingleton().Send(payload);
    }

    void DAVNetworkService::QueueReceivedPacket(std::string packet)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            SKSE::log::warn("DAVSTNET RX dropped: SKSE task interface unavailable");
            return;
        }

        tasks->AddTask([this, packet = std::move(packet)]() mutable {
            HandleReceivedPacketOnGameThread(std::move(packet));
        });
    }

    void DAVNetworkService::HandleReceivedPacketOnGameThread(std::string packet)
    {
        const auto message = DecodeArmorState(packet);
        if (!message) {
            SKSE::log::warn("DAVSTNET RX malformed DAVSTATE packet");
            return;
        }

        auto* resolvedArmorForm = message->armor.Resolve();
        auto* resolvedArmor = resolvedArmorForm ? resolvedArmorForm->As<RE::TESObjectARMO>() : nullptr;
        if (resolvedArmor) {
            message->armor.runtimeFormID = resolvedArmor->GetFormID();
        }

        bool activeValid = true;
        for (auto& identity : message->activeArmorAddons) {
            auto* resolved = identity.Resolve();
            auto* addon = resolved ? resolved->As<RE::TESObjectARMA>() : nullptr;
            if (!addon) {
                activeValid = false;
                identity.runtimeFormID = 0;
                continue;
            }
            identity.runtimeFormID = addon->GetFormID();
        }

        const bool valid = resolvedArmor != nullptr && activeValid;
        SKSE::log::info(
            "DAVSTNET RX_STATE from=\"{}\" armoStable=\"{}\" state={} armoResolved={:08X} activeStable={} activeResolved={} valid={} apply=0",
            message->sender,
            message->armor.StableKey(),
            NetworkArmorStateName(message->state),
            message->armor.runtimeFormID,
            FormatStableIdentities(message->activeArmorAddons),
            FormatRuntimeIDs(message->activeArmorAddons),
            valid ? 1 : 0);
    }
}
