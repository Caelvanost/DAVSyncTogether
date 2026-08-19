#pragma once

#include "NetworkProtocol.h"

namespace DAVSyncTogether
{
    class DAVNetworkService
    {
    public:
        static DAVNetworkService& GetSingleton();

        bool Start();
        void Stop();
        void SendArmorState(const WornArmorState& armor, bool unequipped = false);

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return _running.load();
        }

    private:
        DAVNetworkService() = default;
        ~DAVNetworkService();
        DAVNetworkService(const DAVNetworkService&) = delete;
        DAVNetworkService& operator=(const DAVNetworkService&) = delete;

        void QueueReceivedPacket(std::string packet);
        void HandleReceivedPacketOnGameThread(std::string packet);

        std::atomic_bool _running{ false };
    };
}
