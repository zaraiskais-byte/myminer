#include <cassert>
#include <cstdint>
#include <filesystem>

#include <caesar/node.hpp>

int main() {
    const auto data_dir =
        std::filesystem::temp_directory_path() /
        "caesar_node_test";

    std::error_code ec;
    std::filesystem::remove_all(data_dir, ec);

    constexpr std::uint16_t port = 39425;

    {
        caesar::CaesarNode node(data_dir, port, 1);

        assert(!node.running());
        assert(node.peer_count() == 0);

        node.start();

        assert(node.running());
        assert(node.peer_count() == 0);
        assert(node.height() == 0);

        const auto chain = node.chain();
        assert(chain.size() == 1);
        assert(chain.front().header.height == 0);
        assert(caesar::validate_block_chain(chain));

        node.mine_one_block(
            "CAESAR_NODE_TEST_MINER",
            1000000);

        const auto mined_chain = node.chain();
        assert(mined_chain.size() == 2);
        assert(mined_chain.back().header.height == 1);
        assert(
            mined_chain.back().header.previous_hash ==
            mined_chain.front().hash());
        assert(mined_chain.back().validate_pow());
        assert(caesar::validate_block_chain(mined_chain));

        node.stop();

        assert(!node.running());
        assert(node.peer_count() == 0);
    }

    {
        caesar::CaesarNode reloaded(data_dir, port, 1);

        reloaded.start();

        assert(reloaded.running());
        assert(reloaded.height() == 1);

        const auto reloaded_chain = reloaded.chain();
        assert(reloaded_chain.size() == 2);
        assert(reloaded_chain.back().header.height == 1);
        assert(reloaded_chain.back().validate_pow());
        assert(caesar::validate_block_chain(reloaded_chain));

        reloaded.stop();
        assert(!reloaded.running());
    }

    std::filesystem::remove_all(data_dir, ec);

    return 0;
}
