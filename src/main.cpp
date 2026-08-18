#include "PCH.h"

#include "AppearanceSnapshot.h"
#include "RaceMenuProbe.h"

namespace
{
    std::jthread g_delayedProbeThread;

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

    void ProbeLoadedPlayer(std::string_view reason)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            SKSE::log::error("{}: PlayerCharacter is unavailable", reason);
            return;
        }

        auto& probe = DAVSyncTogether::RaceMenuProbe::GetSingleton();
        auto snapshot = probe.CaptureLocalPlayer(player);

        const auto before = snapshot.faceMorphs.size();
        std::erase_if(snapshot.faceMorphs, [](const DAVSyncTogether::MorphValue& morph) {
            return !std::isfinite(morph.value) ||
                   morph.value == std::numeric_limits<float>::max();
        });
        if (snapshot.faceMorphs.size() != before) {
            SKSE::log::info(
                "DAVST FACE_MORPH filteredSentinels={}",
                before - snapshot.faceMorphs.size());
        }

        SKSE::log::info("DAVST PROBE reason={}", reason);
        probe.LogSnapshot(snapshot, player);
    }

    void ScheduleDelayedProbe()
    {
        if (g_delayedProbeThread.joinable()) {
            g_delayedProbeThread.request_stop();
            g_delayedProbeThread.join();
        }

        g_delayedProbeThread = std::jthread([](std::stop_token token) {
            constexpr auto slice = std::chrono::milliseconds(100);
            constexpr auto delay = std::chrono::seconds(2);
            auto elapsed = std::chrono::milliseconds(0);

            while (elapsed < delay && !token.stop_requested()) {
                std::this_thread::sleep_for(slice);
                elapsed += slice;
            }

            if (token.stop_requested()) {
                return;
            }

            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                SKSE::log::error("DAVST delayed probe: SKSE task interface unavailable");
                return;
            }

            tasks->AddTask([]() {
                ProbeLoadedPlayer("PostLoadGame+2s");
            });
        });
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
            ProbeLoadedPlayer("PostLoadGame");
            ScheduleDelayedProbe();
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
