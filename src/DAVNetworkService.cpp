#include "DAVNetworkService.h"

#include "DAVConfigIndex.h"
#include "DAVProbe.h"
#include "DAVRemoteApplier.h"

namespace DAVSyncTogether
{
    namespace
    {
        std::string FormatStableIdentities(const std::vector<FormIdentity>& values)
        {
            std::string result = "[";
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0) result += ',';
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
                if (i != 0) result += ',';
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
        if (_running.load()) return true;

        _api = STRPM::LoadFromModule();
        if (!_api) {
            SKSE::log::warn("DAVST STRPM messaging API unavailable");
            return false;
        }

        _resolver = STRPM::LoadProxyResolverFromModule();
        if (!_resolver) {
            SKSE::log::warn("DAVST STRPM ProxyResolver unavailable; remote application requires proxy mapping");
        }

        const auto result = _api->registerChannel(kChannel, &DAVNetworkService::OnMessage, this, std::addressof(_listener));
        if (result != STRPM::Result::kOk) {
            SKSE::log::error("DAVST STRPM registerChannel failed channel=\"{}\" result={}", kChannel, STRPM::ResultToString(result));
            _api = nullptr;
            _resolver = nullptr;
            _listener = {};
            return false;
        }

        _running.store(true);

        if (_resolver) {
            const auto listenerResult = _resolver->registerListener(&DAVNetworkService::OnProxyMapping, this);
            if (listenerResult != STRPM::Result::kOk) {
                SKSE::log::warn(
                    "DAVST STRPM proxy-listener registration failed result={}",
                    STRPM::ResultToString(listenerResult));
            } else {
                SKSE::log::info("DAVST STRPM proxy-listener registered");
            }
        }

        UpdateLocalDisplayName();

        STRPM::ConnectionID localConnectionID = 0;
        const auto idResult = _api->getLocalConnectionID(std::addressof(localConnectionID));
        SKSE::log::info(
            "DAVST STRPM ready channel=\"{}\" localConnection={} idResult={} proxyResolver={} apply=1 mode=dav-papyrus",
            kChannel,
            localConnectionID,
            STRPM::ResultToString(idResult),
            _resolver ? 1 : 0);
        return true;
    }

    void DAVNetworkService::Stop()
    {
        if (!_running.exchange(false)) return;
        if (_resolver) {
            const auto listenerResult = _resolver->unregisterListener(&DAVNetworkService::OnProxyMapping, this);
            if (listenerResult != STRPM::Result::kOk && listenerResult != STRPM::Result::kNotAvailable) {
                SKSE::log::warn(
                    "DAVST STRPM proxy-listener unregister failed result={}",
                    STRPM::ResultToString(listenerResult));
            }
        }
        if (_api && _listener.value != 0) {
            const auto result = _api->unregisterChannel(_listener);
            if (result != STRPM::Result::kOk) {
                SKSE::log::warn("DAVST STRPM unregisterChannel failed result={}", STRPM::ResultToString(result));
            }
        }
        _listener = {};
        _resolver = nullptr;
        _api = nullptr;
    }

    void DAVNetworkService::SendArmorState(const WornArmorState& armor, std::string_view variant, bool unequipped)
    {
        STRPM::Target target{};
        target.kind = STRPM::TargetKind::kAllPlayers;
        SendArmorStateImpl(target, armor, variant, unequipped, false);
    }

    void DAVNetworkService::SendArmorStateTo(
        STRPM::ConnectionID connectionID,
        const WornArmorState& armor,
        std::string_view variant,
        bool unequipped)
    {
        if (connectionID == 0) return;

        STRPM::Target target{};
        target.kind = STRPM::TargetKind::kPlayer;
        target.connectionID = connectionID;
        SendArmorStateImpl(target, armor, variant, unequipped, true);
    }

    void DAVNetworkService::SendArmorStateImpl(
        STRPM::Target target,
        const WornArmorState& armor,
        std::string_view variant,
        bool unequipped,
        bool replay)
    {
        if (!_running.load() || !_api || !armor.armor.IsStable()) return;

        if (!DAVConfigIndex::GetSingleton().IsArmorRelevant(armor)) {
            SKSE::log::info(
                "DAVST STRPM TX_FILTERED armoStable=\"{}\" state={} reason=not-in-dav-config",
                armor.armor.StableKey(),
                unequipped ? "UNEQUIPPED" : ArmorVisualStateName(armor.visualState));
            return;
        }

        UpdateLocalDisplayName();
        const auto payload = EncodeArmorState(armor, variant, unequipped);
        constexpr std::uint32_t flags = STRPM::kMessageReliable | STRPM::kMessageOrdered;

        const auto result = _api->send(kChannel, target, payload.data(), payload.size(), flags);
        if (replay) {
            SKSE::log::info(
                "DAVST STRPM TX_REPLAY connection={} armoStable=\"{}\" state={} variant=\"{}\" bytes={} result={}",
                target.connectionID,
                armor.armor.StableKey(),
                unequipped ? "UNEQUIPPED" : ArmorVisualStateName(armor.visualState),
                variant,
                payload.size(),
                STRPM::ResultToString(result));
        } else {
            SKSE::log::info(
                "DAVST STRPM TX armoStable=\"{}\" state={} variant=\"{}\" bytes={} result={}",
                armor.armor.StableKey(),
                unequipped ? "UNEQUIPPED" : ArmorVisualStateName(armor.visualState),
                variant,
                payload.size(),
                STRPM::ResultToString(result));
        }
    }

    void STRPM_CALL DAVNetworkService::OnProxyMapping(const STRPM::ProxyMappingEvent* event, void* userData)
    {
        if (!event || !userData) return;

        auto* service = static_cast<DAVNetworkService*>(userData);
        if (!service->_running.load()) return;

        const bool replayRelevant =
            event->type == STRPM::ProxyMappingEventType::kAdded ||
            event->type == STRPM::ProxyMappingEventType::kUpdated;
        if (!replayRelevant || event->connectionID == 0 ||
            event->newFormID == STRPM::kInvalidProxyFormID) {
            return;
        }

        SKSE::log::info(
            "DAVST PROXY_MAPPING type={} connection={} oldForm={:08X} newForm={:08X} action=queue-state-replay",
            event->type == STRPM::ProxyMappingEventType::kAdded ? "ADDED" : "UPDATED",
            event->connectionID,
            event->oldFormID,
            event->newFormID);

        DAVProbe::GetSingleton().QueueStateReplay(event->connectionID);
    }

    void STRPM_CALL DAVNetworkService::OnMessage(const STRPM::Message* message, void* userData)
    {
        if (!message || !userData || !message->data || message->size == 0) return;
        auto* service = static_cast<DAVNetworkService*>(userData);
        ReceivedMessage received;
        received.payload.assign(static_cast<const char*>(message->data), message->size);
        received.connectionID = message->sender.connectionID;
        received.displayName = message->sender.displayName ? message->sender.displayName : "<unknown>";
        received.isHost = message->sender.isHost;
        received.sequence = message->sequence;
        service->QueueReceivedMessage(std::move(received));
    }

    void DAVNetworkService::QueueReceivedMessage(ReceivedMessage message)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            SKSE::log::warn("DAVST STRPM RX dropped: SKSE task interface unavailable");
            return;
        }
        tasks->AddTask([this, message = std::move(message)]() mutable {
            HandleReceivedMessageOnGameThread(std::move(message));
        });
    }

    void DAVNetworkService::HandleReceivedMessageOnGameThread(ReceivedMessage received)
    {
        auto message = DecodeArmorState(received.payload);
        if (!message) {
            SKSE::log::warn("DAVST STRPM RX malformed payload connection={} sequence={}", received.connectionID, received.sequence);
            return;
        }

        auto* resolvedArmorForm = message->armor.Resolve();
        auto* resolvedArmor = resolvedArmorForm ? resolvedArmorForm->As<RE::TESObjectARMO>() : nullptr;
        if (resolvedArmor) message->armor.runtimeFormID = resolvedArmor->GetFormID();

        bool activeValid = true;
        for (auto& identity : message->activeArmorAddons) {
            auto* resolved = identity.Resolve();
            auto* addon = resolved ? resolved->As<RE::TESObjectARMA>() : nullptr;
            if (!addon) {
                activeValid = false;
                identity.runtimeFormID = 0;
            } else {
                identity.runtimeFormID = addon->GetFormID();
            }
        }

        STRPM::ProxyFormID proxyFormID = STRPM::kInvalidProxyFormID;
        STRPM::Result proxyResult = STRPM::Result::kNotAvailable;
        if (_resolver) proxyResult = _resolver->resolve(received.connectionID, std::addressof(proxyFormID));

        RE::Actor* proxyActor = nullptr;
        if (proxyResult == STRPM::Result::kOk && proxyFormID != STRPM::kInvalidProxyFormID) {
            if (auto* form = RE::TESForm::LookupByID(proxyFormID)) proxyActor = form->As<RE::Actor>();
        }

        const bool valid = resolvedArmor != nullptr && activeValid;
        RemoteApplyResult applyResult{};
        if (valid && proxyActor) applyResult = DAVRemoteApplier::Apply(proxyActor, *message);

        SKSE::log::info(
            "DAVST STRPM RX_STATE sender=\"{}\" connection={} host={} sequence={} proxyResult={} proxyForm={:08X} proxyActor={} armoStable=\"{}\" state={} variant=\"{}\" armoResolved={:08X} activeStable={} activeResolved={} valid={} applySupported={} davDispatch={} fallbackNodes={} apply={}",
            received.displayName,
            received.connectionID,
            received.isHost ? 1 : 0,
            received.sequence,
            STRPM::ResultToString(proxyResult),
            proxyFormID,
            proxyActor ? 1 : 0,
            message->armor.StableKey(),
            NetworkArmorStateName(message->state),
            message->variant,
            message->armor.runtimeFormID,
            FormatStableIdentities(message->activeArmorAddons),
            FormatRuntimeIDs(message->activeArmorAddons),
            valid ? 1 : 0,
            applyResult.supported ? 1 : 0,
            applyResult.dispatched ? 1 : 0,
            applyResult.fallbackNodes,
            (valid && proxyActor && applyResult.supported && (applyResult.dispatched || applyResult.fallbackNodes > 0)) ? 1 : 0);
    }

    void DAVNetworkService::UpdateLocalDisplayName()
    {
        if (!_api) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        const char* name = player->GetName();
        if (!name || !*name || std::string_view(name) == "Prisoner") return;
        const auto result = _api->setLocalDisplayName(name);
        if (result != STRPM::Result::kOk) {
            SKSE::log::trace("DAVST STRPM setLocalDisplayName result={}", STRPM::ResultToString(result));
        }
    }
}
