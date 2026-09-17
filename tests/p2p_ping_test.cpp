#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <caesar/p2p_ping.hpp>

int main() {
    using namespace caesar;

    P2PPing ping;
    ping.nonce = 123456789;

    const auto encoded_ping = ping.serialize_binary();
    const auto decoded_ping =
        P2PPing::deserialize_binary(encoded_ping);

    assert(decoded_ping.nonce == ping.nonce);

    P2PPong pong;
    pong.nonce = decoded_ping.nonce;

    const auto encoded_pong = pong.serialize_binary();
    const auto decoded_pong =
        P2PPong::deserialize_binary(encoded_pong);

    assert(decoded_pong.nonce == ping.nonce);

    auto bad = encoded_ping;
    bad.push_back(0);

    bool rejected = false;

    try {
        (void)P2PPing::deserialize_binary(bad);
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);

    return 0;
}
