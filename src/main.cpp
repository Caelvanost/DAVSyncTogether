#include "PCH.h"

#include "AppearanceSnapshot.h"
#include "RaceMenuProbe.h"

namespace
{
    void InitLogging()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }

        *path /= "DAVSyncTogether.log";

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            path->string(),
            true);

        auto log = std::make_shared<spdlog::logger>(
            "DAVSyncTogether",
            std::move(sink));

        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
    }

    void ProbeLoadedPlayer()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            SKSE::log::error("PostLoadGame: PlayerCharacter is unavailable");
            return;
        }

        auto& probe = DAVSyncTogether::RaceMenuProbe::GetSingleton();
        auto snapshot = probe.CaptureLocalPlayer(player);
        probe.LogSnapshot(snapshot, player);
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        switch (message->type) {
        case SKSE::MessagingInterface::kPostPostLoad:
            SKSE::log::info("PostPostLoad: querying RaceMenu/SKEE interfaces");
            DAVSyncTogether::RaceMenuProbe::GetSingleton().Initialize();
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            SKSE::log::info("DataLoaded: DAVSync Together core ready; waiting for loaded player appearance");
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
            SKSE::log::info("PostLoadGame: capturing local appearance probe");
            ProbeLoadedPlayer();
            break;

        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    InitLogging();
    SKSE::Init(skse);

    SKSE::log::info("DAVSyncTogether v{} loading", DAVST_VERSION_STRING);

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        SKSE::log::critical("No SKSE messaging interface");
        return false;
    }

    messaging->RegisterListener(OnSKSEMessage);
    return true;
}
