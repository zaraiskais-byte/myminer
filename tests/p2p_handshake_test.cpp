#include <cassert>
#include <cstdint>

#include <caesar/p2p_handshake.hpp>

int main() {

    caesar::P2PHello hello;
    hello.protocol_version =
        caesar::CZR_P2P_PROTOCOL_VERSION;
    hello.network_id = 1;
    hello.height = 123;
    hello.timestamp = 456;
    hello.user_agent = "Caesar-CZR-Test";

    caesar::P2PFrame frame =
        caesar::make_hello_frame(hello);

    assert(frame.type ==
           caesar::P2PMessageType::Hello);

    caesar::P2PHello decoded =
        caesar::parse_hello_frame(frame);

    assert(decoded.protocol_version ==
           hello.protocol_version);
    assert(decoded.network_id ==
           hello.network_id);
    assert(decoded.height ==
           hello.height);
    assert(decoded.timestamp ==
           hello.timestamp);
    assert(decoded.user_agent ==
           hello.user_agent);

    assert(caesar::validate_hello(
        decoded, 1));

    assert(!caesar::validate_hello(
        decoded, 2));

    decoded.protocol_version++;

    assert(!caesar::validate_hello(
        decoded, 1));

    return 0;
}
