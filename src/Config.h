#pragma once

#include "PCH.h"

namespace DAVSyncTogether
{
    struct Config
    {
        bool networkEnabled{ true };
        bool autoDiscovery{ true };
        std::uint16_t localPort{ 38472 };
        std::uint16_t peerPort{ 38472 };
        std::uint32_t discoveryIntervalMs{ 2000 };
        std::uint32_t peerTimeoutMs{ 10000 };
        std::string peerHost{ "127.0.0.1" };

        static Config Load();
    };
}
