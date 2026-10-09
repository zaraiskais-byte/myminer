#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/p2p_peer_discovery.hpp>

using namespace caesar;

int main() {
    {
        const auto frame = make_get_peers_frame();

        assert(frame.type == P2PMessageType::GetPeers);
        assert(frame.payload.empty());

        const auto parsed = parse_get_peers_payload(frame.payload);
        assert(parsed.empty());
    }

    {
        const std::vector<P2PPeerEndpoint> input{
            {"198.51.100.10", 18444},
            {"203.0.113.20", 18444},
        };

        const auto frame = make_peers_frame(input);

        assert(frame.type == P2PMessageType::Peers);

        const auto parsed = parse_peers_payload(frame.payload);

        assert(parsed.size() == 2);
        assert(parsed[0].address == "198.51.100.10");
        assert(parsed[0].port == 18444);
        assert(parsed[1].address == "203.0.113.20");
        assert(parsed[1].port == 18444);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame(
                std::vector<P2PPeerEndpoint>(
                    CZR_P2P_MAX_DISCOVERY_PEERS + 1,
                    {"198.51.100.10", 18444}));
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            parse_get_peers_payload({0x01});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame({{"", 18444}});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame({{"198.51.100.10", 0}});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame({{"not-an-ip", 18444}});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame({{"0.0.0.0", 18444}});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame({{"127.0.0.1", 18444}});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        bool rejected = false;

        try {
            make_peers_frame({{"224.0.0.1", 18444}});
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        const std::vector<P2PPeerEndpoint> valid{
            {"10.0.0.5", 18444},
            {"192.168.1.20", 18444},
            {"198.51.100.10", 18444},
        };

        const auto frame = make_peers_frame(valid);
        const auto parsed = parse_peers_payload(frame.payload);

        assert(parsed.size() == valid.size());
        assert(parsed[0].address == "10.0.0.5");
        assert(parsed[1].address == "192.168.1.20");
        assert(parsed[2].address == "198.51.100.10");
    }

    {
        bool rejected = false;

        try {
            std::vector<std::uint8_t> malformed{
                1, 0, 0, 0
            };

            parse_peers_payload(malformed);
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    return 0;
}
