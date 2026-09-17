#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <caesar/p2p_hello.hpp>

int main() {
    using namespace caesar;

    P2PHello hello;
    hello.protocol_version = CZR_P2P_PROTOCOL_VERSION;
    hello.network_id = 1;
    hello.height = 12345;
    hello.timestamp = 1770000000;
    hello.user_agent = "Caesar-CZR-Test";

    const auto encoded = hello.serialize_binary();
    const auto decoded = P2PHello::deserialize_binary(encoded);

    assert(decoded.protocol_version == hello.protocol_version);
    assert(decoded.network_id == hello.network_id);
    assert(decoded.height == hello.height);
    assert(decoded.timestamp == hello.timestamp);
    assert(decoded.user_agent == hello.user_agent);

    {
        P2PHello oversized;
        oversized.user_agent.assign(
            static_cast<std::size_t>(CZR_P2P_MAX_USER_AGENT) + 1,
            'X');

        bool rejected = false;

        try {
            (void)oversized.serialize_binary();
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        auto bad = encoded;
        bad.push_back(0);

        bool rejected = false;

        try {
            (void)P2PHello::deserialize_binary(bad);
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    return 0;
}
