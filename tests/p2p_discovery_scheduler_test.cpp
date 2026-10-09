#include <cassert>
#include <cstddef>

#include <caesar/p2p_server.hpp>

int main() {
    caesar::P2PServer server;

    /*
     * The discovery scheduler must remain inert while
     * the P2P server is stopped.
     */
    assert(server.connect_to_discovered_peers() == 0);
    assert(server.connect_to_discovered_peers(0) == 0);

    /*
     * No outbound connection may be created merely by
     * invoking the scheduler while stopped.
     */
    assert(server.peer_count() == 0);

    server.start(0, "127.0.0.1");
    assert(server.running());
    server.stop();
    assert(!server.running());
    assert(server.peer_count() == 0);

    server.stop();
    assert(!server.running());

    server.start(0, "127.0.0.1");
    assert(server.running());
    server.stop();
    assert(!server.running());

    return 0;
}
