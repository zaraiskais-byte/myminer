#include <string>
#include <cassert>
#include <caesar/p2p_protocol.hpp>

int main() {
    using namespace caesar;

    assert(static_cast<std::uint8_t>(P2PMessageType::Hello) == 1);
    assert(static_cast<std::uint8_t>(P2PMessageType::Ping) == 2);
    assert(static_cast<std::uint8_t>(P2PMessageType::Pong) == 3);
    assert(static_cast<std::uint8_t>(P2PMessageType::Transaction) == 9);
    assert(static_cast<std::uint8_t>(P2PMessageType::Reject) == 11);

    assert(std::string(p2p_message_name(P2PMessageType::Hello)) == "hello");
    assert(std::string(p2p_message_name(P2PMessageType::Ping)) == "ping");
    assert(std::string(p2p_message_name(P2PMessageType::Transaction)) == "transaction");
    assert(std::string(p2p_message_name(P2PMessageType::Reject)) == "reject");

    assert(std::string(p2p_message_name(
        static_cast<P2PMessageType>(255))) == "unknown");

    return 0;
}
