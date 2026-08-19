#include "UdpTransport.h"

namespace DAVSyncTogether
{
    namespace
    {
        constexpr std::string_view kDiscoveryPrefix = "DAVSTDISC|v1|HELLO|";
        constexpr std::string_view kGameplayPrefix = "DAVSTNET|v1|";
    }

    UdpTransport& UdpTransport::GetSingleton()
    {
        static UdpTransport instance;
        return instance;
    }

    UdpTransport::~UdpTransport()
    {
        Stop();
    }

    std::string UdpTransport::SanitizeField(std::string value)
    {
        for (auto& ch : value) {
            if (ch == '|' || ch == '\r' || ch == '\n') {
                ch = '_';
            }
        }
        return value.empty() ? "Player" : value;
    }

    std::string UdpTransport::GetLocalPlayerName() const
    {
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            const auto* name = player->GetName();
            if (name && *name) {
                return SanitizeField(name);
            }
        }
        return "Player";
    }

    std::optional<std::string> UdpTransport::ReadField(
        std::string_view packet,
        std::string_view key)
    {
        const auto needle = fmt::format("{}=", key);
        auto position = packet.find(needle);
        if (position == std::string_view::npos) {
            return std::nullopt;
        }
        position += needle.size();
        auto end = packet.find('|', position);
        if (end == std::string_view::npos) {
            end = packet.size();
        }
        return std::string(packet.substr(position, end - position));
    }

    std::string UdpTransport::AddressToString(const sockaddr_in& address)
    {
        std::array<char, INET_ADDRSTRLEN> ip{};
        InetNtopA(
            AF_INET,
            &address.sin_addr,
            ip.data(),
            static_cast<DWORD>(ip.size()));
        return fmt::format("{}:{}", ip.data(), ntohs(address.sin_port));
    }

    bool UdpTransport::Start(const Config& config, PacketHandler handler)
    {
        if (_running.load()) {
            return true;
        }
        if (!config.networkEnabled || !handler) {
            SKSE::log::info("DAVSTNET transport disabled");
            return false;
        }

        _config = config;
        _handler = std::move(handler);

        WSADATA wsa{};
        if (const auto result = WSAStartup(MAKEWORD(2, 2), &wsa); result != 0) {
            SKSE::log::error("DAVSTNET WSAStartup failed: {}", result);
            return false;
        }

        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_socket == INVALID_SOCKET) {
            SKSE::log::error("DAVSTNET socket creation failed: {}", WSAGetLastError());
            WSACleanup();
            return false;
        }

        BOOL enabled = TRUE;
        setsockopt(
            _socket,
            SOL_SOCKET,
            SO_BROADCAST,
            reinterpret_cast<const char*>(&enabled),
            sizeof(enabled));
        setsockopt(
            _socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&enabled),
            sizeof(enabled));

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(_config.localPort);
        if (bind(_socket, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
            SKSE::log::error(
                "DAVSTNET bind failed on port {}: {}",
                _config.localPort,
                WSAGetLastError());
            closesocket(_socket);
            _socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }

        DWORD timeout = 250;
        setsockopt(
            _socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout),
            sizeof(timeout));

        _broadcast = {};
        _broadcast.sin_family = AF_INET;
        _broadcast.sin_port = htons(_config.localPort);
        _broadcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        _hasManualPeer = false;
        if (!_config.autoDiscovery && !_config.peerHost.empty()) {
            _manualPeer = {};
            _manualPeer.sin_family = AF_INET;
            _manualPeer.sin_port = htons(_config.peerPort);
            _hasManualPeer = InetPtonA(
                AF_INET,
                _config.peerHost.c_str(),
                &_manualPeer.sin_addr) == 1;
            if (!_hasManualPeer) {
                SKSE::log::error("DAVSTNET invalid PeerHost: {}", _config.peerHost);
                closesocket(_socket);
                _socket = INVALID_SOCKET;
                WSACleanup();
                return false;
            }
        }

        _instanceID = fmt::format("{:08X}-{:016X}", GetCurrentProcessId(), GetTickCount64());
        _running.store(true);
        _receiver = std::jthread([this](std::stop_token) { ReceiverLoop(); });
        if (_config.autoDiscovery) {
            _discovery = std::jthread([this](std::stop_token token) {
                DiscoveryLoop(token);
            });
            SendHello();
        }

        SKSE::log::info(
            "DAVSTNET started player=\"{}\" port={} discovery={} instance={}",
            GetLocalPlayerName(),
            _config.localPort,
            _config.autoDiscovery ? 1 : 0,
            _instanceID);
        return true;
    }

    void UdpTransport::Stop()
    {
        if (!_running.exchange(false)) {
            return;
        }

        if (_receiver.joinable()) {
            _receiver.request_stop();
        }
        if (_discovery.joinable()) {
            _discovery.request_stop();
        }
        if (_socket != INVALID_SOCKET) {
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
        if (_receiver.joinable()) {
            _receiver.join();
        }
        if (_discovery.joinable()) {
            _discovery.join();
        }

        {
            std::scoped_lock lock(_peerMutex);
            _peers.clear();
        }
        _handler = {};
        WSACleanup();
        SKSE::log::info("DAVSTNET stopped");
    }

    void UdpTransport::SendHello()
    {
        if (_running.load() && _config.autoDiscovery) {
            SendHelloTo(_broadcast);
        }
    }

    void UdpTransport::SendHelloTo(const sockaddr_in& destination)
    {
        if (_socket == INVALID_SOCKET) {
            return;
        }

        const auto packet = fmt::format(
            "DAVSTDISC|v1|HELLO|id={}|name={}|port={}",
            _instanceID,
            GetLocalPlayerName(),
            _config.localPort);

        std::scoped_lock lock(_sendMutex);
        sendto(
            _socket,
            packet.data(),
            static_cast<int>(packet.size()),
            0,
            reinterpret_cast<const sockaddr*>(&destination),
            sizeof(destination));
    }

    bool UdpTransport::HandleDiscovery(
        std::string_view packet,
        const sockaddr_in& source)
    {
        if (!packet.starts_with(kDiscoveryPrefix)) {
            return false;
        }

        const auto id = ReadField(packet, "id");
        const auto name = ReadField(packet, "name");
        const auto portText = ReadField(packet, "port");
        if (!id || !name || !portText || *id == _instanceID) {
            return true;
        }

        unsigned long port = 0;
        try {
            port = std::stoul(*portText);
        } catch (...) {
            return true;
        }
        if (port == 0 || port > 65535) {
            return true;
        }

        sockaddr_in peerAddress = source;
        peerAddress.sin_port = htons(static_cast<std::uint16_t>(port));
        const auto key = fmt::format("{}|{}", *id, AddressToString(peerAddress));
        bool inserted = false;
        {
            std::scoped_lock lock(_peerMutex);
            auto [iterator, isNew] = _peers.try_emplace(key);
            inserted = isNew;
            iterator->second.address = peerAddress;
            iterator->second.name = *name;
            iterator->second.lastSeen = std::chrono::steady_clock::now();
        }
        if (inserted) {
            SKSE::log::info(
                "DAVSTNET DISCOVERED peer=\"{}\" addr={}",
                *name,
                AddressToString(peerAddress));
            SendHelloTo(peerAddress);
        }
        return true;
    }

    void UdpTransport::TouchGameplayPeer(
        std::string_view packet,
        const sockaddr_in& source)
    {
        if (!packet.starts_with(kGameplayPrefix)) {
            return;
        }

        const auto id = ReadField(packet, "id");
        const auto name = ReadField(packet, "from");
        const auto key = fmt::format(
            "{}|{}",
            id.value_or("gameplay"),
            AddressToString(source));

        std::scoped_lock lock(_peerMutex);
        auto& peer = _peers[key];
        peer.address = source;
        peer.name = name.value_or("Peer");
        peer.lastSeen = std::chrono::steady_clock::now();
    }

    void UdpTransport::ExpirePeers()
    {
        const auto now = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(_config.peerTimeoutMs);
        std::scoped_lock lock(_peerMutex);
        for (auto iterator = _peers.begin(); iterator != _peers.end();) {
            if (now - iterator->second.lastSeen > timeout) {
                SKSE::log::info(
                    "DAVSTNET PEER EXPIRED peer=\"{}\" addr={}",
                    iterator->second.name,
                    AddressToString(iterator->second.address));
                iterator = _peers.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    std::vector<sockaddr_in> UdpTransport::SnapshotPeers()
    {
        std::vector<sockaddr_in> result;
        std::unordered_set<std::string> endpoints;
        std::scoped_lock lock(_peerMutex);
        for (const auto& [key, peer] : _peers) {
            const auto endpoint = AddressToString(peer.address);
            if (endpoints.insert(endpoint).second) {
                result.push_back(peer.address);
            }
        }
        return result;
    }

    void UdpTransport::Send(std::string_view payload)
    {
        if (!_running.load() || _socket == INVALID_SOCKET || payload.empty()) {
            return;
        }

        const auto packet = fmt::format(
            "DAVSTNET|v1|id={}|from={}|{}",
            _instanceID,
            GetLocalPlayerName(),
            payload);

        std::vector<sockaddr_in> destinations;
        if (_config.autoDiscovery) {
            destinations = SnapshotPeers();
            if (destinations.empty()) {
                destinations.push_back(_broadcast);
            }
        } else if (_hasManualPeer) {
            destinations.push_back(_manualPeer);
        }

        if (destinations.empty()) {
            SKSE::log::warn("DAVSTNET TX dropped: no destination");
            return;
        }

        std::scoped_lock lock(_sendMutex);
        std::size_t sentCount = 0;
        for (const auto& destination : destinations) {
            const auto sent = sendto(
                _socket,
                packet.data(),
                static_cast<int>(packet.size()),
                0,
                reinterpret_cast<const sockaddr*>(&destination),
                sizeof(destination));
            if (sent == SOCKET_ERROR) {
                SKSE::log::warn(
                    "DAVSTNET TX failed to {}: {}",
                    AddressToString(destination),
                    WSAGetLastError());
                continue;
            }
            ++sentCount;
        }

        SKSE::log::info("DAVSTNET TX peers={} {}", sentCount, packet);
    }

    void UdpTransport::DiscoveryLoop(std::stop_token token)
    {
        while (!token.stop_requested() && _running.load()) {
            SendHello();
            ExpirePeers();

            const auto interval = std::chrono::milliseconds(_config.discoveryIntervalMs);
            auto elapsed = std::chrono::milliseconds(0);
            while (elapsed < interval && !token.stop_requested() && _running.load()) {
                constexpr auto slice = std::chrono::milliseconds(100);
                std::this_thread::sleep_for(slice);
                elapsed += slice;
            }
        }
    }

    void UdpTransport::ReceiverLoop()
    {
        std::array<char, 8192> buffer{};
        while (_running.load()) {
            sockaddr_in source{};
            int sourceLength = sizeof(source);
            const auto received = recvfrom(
                _socket,
                buffer.data(),
                static_cast<int>(buffer.size() - 1),
                0,
                reinterpret_cast<sockaddr*>(&source),
                &sourceLength);

            if (received == SOCKET_ERROR) {
                const auto error = WSAGetLastError();
                if (!_running.load()) {
                    break;
                }
                if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
                    continue;
                }
                SKSE::log::warn("DAVSTNET RX failed: {}", error);
                continue;
            }
            if (received <= 0) {
                continue;
            }

            const std::string packet(buffer.data(), static_cast<std::size_t>(received));
            if (_config.autoDiscovery && HandleDiscovery(packet, source)) {
                continue;
            }
            if (!packet.starts_with(kGameplayPrefix)) {
                continue;
            }
            if (const auto id = ReadField(packet, "id"); id && *id == _instanceID) {
                continue;
            }

            TouchGameplayPeer(packet, source);
            SKSE::log::info(
                "DAVSTNET RX_RAW from={} {}",
                AddressToString(source),
                packet);
            if (_handler) {
                _handler(packet);
            }
        }
    }
}
