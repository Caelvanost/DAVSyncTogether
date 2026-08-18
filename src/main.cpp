#include "PCH.h"

#include "AppearanceSnapshot.h"

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

    void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        switch (message->type) {
        case SKSE::MessagingInterface::kPostPostLoad:
            SKSE::log::info("PostPostLoad: appearance integrations may now be queried");
            break;

        case SKSE::MessagingInterface::kDataLoaded:
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                SKSE::log::error("DataLoaded: PlayerCharacter is unavailable");
                break;
            }

            DAVSyncTogether::AppearanceSnapshot snapshot;
            snapshot.playerName = player->GetName();

            if (auto* race = player->GetRace()) {
                snapshot.raceEditorID = race->GetFormEditorID();
            }

            SKSE::log::info("DataLoaded: DAVSync Together core ready; {}", snapshot.Summary());
            break;
        }

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
