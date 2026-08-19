#pragma once

#include "DAVStateSnapshot.h"

namespace DAVSyncTogether
{
    class DAVProbe
    {
    public:
        static DAVProbe& GetSingleton();

        void Start();
        void Stop();
        void Reset();
        void QueueProbe(std::string reason = "tick");

    private:
        DAVProbe() = default;
        ~DAVProbe();
        DAVProbe(const DAVProbe&) = delete;
        DAVProbe& operator=(const DAVProbe&) = delete;

        DAVStateSnapshot CaptureLocalPlayer(RE::PlayerCharacter* player) const;
        void TickOnGameThread(std::string reason);
        void LogSnapshotDiff(const DAVStateSnapshot& current, std::string_view reason);

        static bool IsDAVLoaded();
        static void VisitScene(RE::NiAVObject* object, std::vector<std::string>& names);
        static std::uint64_t HashStrings(const std::vector<std::string>& values) noexcept;

        std::jthread _thread;
        std::atomic_bool _running{ false };
        std::atomic_bool _tickQueued{ false };

        bool _hasPrevious{ false };
        DAVStateSnapshot _previous;
    };
}
