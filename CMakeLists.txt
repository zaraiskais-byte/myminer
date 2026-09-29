cmake_minimum_required(VERSION 3.20)

project(CaesarCZR VERSION 0.1.0 LANGUAGES CXX)

# Android/Termux: use the platform-compatible libunwind explicitly for
# C++ exception cleanup/unwinding.  This is a linker setting, not a
# compiler flag, and is intentionally limited to Android.
if(ANDROID)
    find_library(CAESAR_UNWIND_LIBRARY
        NAMES unwind
        REQUIRED
    )
    link_libraries(${CAESAR_UNWIND_LIBRARY})
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

option(CAESAR_ENABLE_SANITIZERS
    "Enable Clang AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF)

if(CAESAR_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "CAESAR_ENABLE_SANITIZERS requires Clang")
    endif()

    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )

    add_link_options(
        -fsanitize=address,undefined
    )

    message(STATUS
        "CAESAR SANITIZERS: AddressSanitizer + UndefinedBehaviorSanitizer")
endif()

# ---------------------------------------------------------------------------
# ThreadSanitizer (TSan) — opt-in, Clang only.
#
# TSan is complementary to ASan/UBSan: it detects data races and lock
# misuse at runtime, which the address and undefined-behavior sanitizers
# cannot see. The CI runs a dedicated job that builds this option and
# runs the concurrency-sensitive tests.
#
# TSan and ASan cannot be combined in the same binary, so this option
# replaces the ASan/UBSan flags entirely when enabled.
# ---------------------------------------------------------------------------
option(CAESAR_ENABLE_TSAN
    "Build with ThreadSanitizer (Clang only)" OFF)

if(CAESAR_ENABLE_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "CAESAR_ENABLE_TSAN requires Clang")
    endif()

    add_compile_options(
        -fsanitize=thread
        -fno-omit-frame-pointer
        -g
        -O1
    )
    add_link_options(-fsanitize=thread)

    message(STATUS
        "CAESAR TSAN: ThreadSanitizer enabled")
endif()


add_executable(caesard
    src/main.cpp
)

target_include_directories(caesard PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesard PRIVATE OpenSSL::Crypto)

add_executable(caesar_foundation_test
    tests/foundation_test.cpp
)

target_include_directories(caesar_foundation_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_foundation_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_protocol_data_test
    tests/protocol_data_test.cpp
)

target_include_directories(caesar_protocol_data_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_protocol_data_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_mempool_validation_test tests/mempool_validation_test.cpp)
target_include_directories(caesar_mempool_validation_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_mempool_validation_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_mempool_test tests/mempool_test.cpp)
target_include_directories(caesar_mempool_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_mempool_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_block_test tests/block_test.cpp)
target_include_directories(caesar_block_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_block_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_block_builder_test tests/block_builder_test.cpp)
target_include_directories(caesar_block_builder_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_block_builder_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_coinbase_test tests/coinbase_test.cpp)
target_include_directories(caesar_coinbase_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_coinbase_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_consensus_test tests/consensus_test.cpp)
target_include_directories(caesar_consensus_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_consensus_test PRIVATE OpenSSL::Crypto)

enable_testing()
add_test(NAME CaesarConsensusTest COMMAND caesar_consensus_test)
add_test(NAME CaesarCoinbaseTest COMMAND caesar_coinbase_test)
add_test(NAME CaesarBlockBuilderTest COMMAND caesar_block_builder_test)
add_test(NAME CaesarBlockTest COMMAND caesar_block_test)
add_test(NAME CaesarMempoolTest COMMAND caesar_mempool_test)
add_test(NAME CaesarMempoolValidationTest COMMAND caesar_mempool_validation_test)
add_test(NAME CaesarFoundationTest COMMAND caesar_foundation_test)
add_test(NAME CaesarProtocolDataTest COMMAND caesar_protocol_data_test)

add_executable(caesar_transaction_test
    tests/transaction_test.cpp
)

target_include_directories(caesar_transaction_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_transaction_test PRIVATE OpenSSL::Crypto)

add_test(NAME CaesarTransactionTest COMMAND caesar_transaction_test)

add_executable(caesar_signature_test
    tests/signature_test.cpp
)

target_include_directories(caesar_signature_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_signature_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_transaction_signature_test
    tests/transaction_signature_test.cpp
)

target_include_directories(caesar_transaction_signature_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_transaction_signature_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_wallet_test
    tests/wallet_test.cpp
)

target_include_directories(caesar_wallet_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_wallet_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_ownership_test
    tests/ownership_test.cpp
)

target_include_directories(caesar_ownership_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_ownership_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_address_test
    tests/address_test.cpp
)

target_include_directories(caesar_address_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_address_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_key_encoding_test tests/key_encoding_test.cpp)
target_include_directories(caesar_key_encoding_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_key_encoding_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_serialization_test tests/serialization_test.cpp)
target_include_directories(caesar_serialization_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_serialization_test PRIVATE OpenSSL::Crypto)

add_executable(caesar_difficulty_consensus_test
    tests/difficulty_consensus_test.cpp
)

target_include_directories(caesar_difficulty_consensus_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME caesar_difficulty_consensus_test
    COMMAND caesar_difficulty_consensus_test
)

add_executable(caesar_utxo_test
    tests/utxo_test.cpp
)

target_include_directories(caesar_utxo_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(caesar_utxo_test PRIVATE OpenSSL::Crypto)

add_test(NAME CaesarUTXOTest COMMAND caesar_utxo_test)
add_test(NAME CaesarSerializationTest COMMAND caesar_serialization_test)
add_test(NAME CaesarKeyEncodingTest COMMAND caesar_key_encoding_test)
add_test(NAME CaesarAddressTest COMMAND caesar_address_test)
add_test(NAME CaesarOwnershipTest COMMAND caesar_ownership_test)
add_test(NAME CaesarWalletTest COMMAND caesar_wallet_test)
add_test(NAME CaesarTransactionSignatureTest COMMAND caesar_transaction_signature_test)
add_test(NAME CaesarSignatureTest COMMAND caesar_signature_test)


add_executable(caesar_difficulty_block_validation_test
    tests/difficulty_block_validation_test.cpp
)

target_include_directories(caesar_difficulty_block_validation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME caesar_difficulty_block_validation_test
    COMMAND caesar_difficulty_block_validation_test
)


add_executable(caesar_difficulty_blockchain_validation_test
    tests/difficulty_blockchain_validation_test.cpp
)

target_include_directories(caesar_difficulty_blockchain_validation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_difficulty_blockchain_validation_test PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME caesar_difficulty_blockchain_validation_test
    COMMAND caesar_difficulty_blockchain_validation_test
)

add_executable(caesar_full_block_consensus_test
    tests/full_block_consensus_test.cpp
)

target_include_directories(caesar_full_block_consensus_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_full_block_consensus_test PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME CaesarFullBlockConsensusTest
    COMMAND caesar_full_block_consensus_test
)

add_executable(caesar_p2p_protocol_test
    tests/p2p_protocol_test.cpp
)

target_include_directories(caesar_p2p_protocol_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_protocol_test
    PRIVATE
        OpenSSL::Crypto
)

add_test(
    NAME CaesarP2PProtocolTest
    COMMAND caesar_p2p_protocol_test
)

add_executable(caesar_p2p_frame_test
    tests/p2p_frame_test.cpp
)

target_include_directories(caesar_p2p_frame_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_frame_test
    PRIVATE
        OpenSSL::Crypto
)

add_test(
    NAME CaesarP2PFrameTest
    COMMAND caesar_p2p_frame_test
)

add_executable(caesar_p2p_hello_test
    tests/p2p_hello_test.cpp
)
target_include_directories(caesar_p2p_hello_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(caesar_p2p_hello_test PRIVATE OpenSSL::Crypto)
add_test(NAME CaesarP2PHelloTest COMMAND caesar_p2p_hello_test)

add_executable(caesar_p2p_ping_test
    tests/p2p_ping_test.cpp
)
target_include_directories(caesar_p2p_ping_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(caesar_p2p_ping_test PRIVATE OpenSSL::Crypto)
add_test(NAME CaesarP2PPingTest COMMAND caesar_p2p_ping_test)

add_executable(caesar_p2p_tcp_test
    tests/p2p_tcp_test.cpp
)
target_include_directories(caesar_p2p_tcp_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(caesar_p2p_tcp_test PRIVATE OpenSSL::Crypto)
add_test(NAME CaesarP2PTcpTest COMMAND caesar_p2p_tcp_test)

add_executable(caesar_p2p_connection_test
    tests/p2p_connection_test.cpp
)
target_include_directories(caesar_p2p_connection_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(caesar_p2p_connection_test PRIVATE OpenSSL::Crypto)
add_test(NAME CaesarP2PConnectionTest COMMAND caesar_p2p_connection_test)

add_executable(caesar_p2p_peer_manager_test
    tests/p2p_peer_manager_test.cpp
)
target_include_directories(caesar_p2p_peer_manager_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(caesar_p2p_peer_manager_test PRIVATE OpenSSL::Crypto)
add_test(NAME CaesarP2PPeerManagerTest COMMAND caesar_p2p_peer_manager_test)


add_executable(caesar_p2p_server_test tests/p2p_server_test.cpp)
target_include_directories(caesar_p2p_server_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(caesar_p2p_server_test PRIVATE crypto pthread)
add_test(NAME caesar_p2p_server_test COMMAND caesar_p2p_server_test)

add_executable(caesar_p2p_handshake_test tests/p2p_handshake_test.cpp)
target_include_directories(caesar_p2p_handshake_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(caesar_p2p_handshake_test PRIVATE crypto pthread)
add_test(NAME caesar_p2p_handshake_test COMMAND caesar_p2p_handshake_test)

add_executable(caesar_p2p_server_handshake_test tests/p2p_server_handshake_test.cpp)
target_include_directories(caesar_p2p_server_handshake_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(caesar_p2p_server_handshake_test PRIVATE crypto pthread)
add_test(NAME caesar_p2p_server_handshake_test COMMAND caesar_p2p_server_handshake_test)

add_executable(caesar_p2p_server_reject_test tests/p2p_server_reject_test.cpp)
target_include_directories(caesar_p2p_server_reject_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(caesar_p2p_server_reject_test PRIVATE crypto pthread)
add_test(NAME caesar_p2p_server_reject_test COMMAND caesar_p2p_server_reject_test)

add_executable(caesar_p2p_server_version_reject_test tests/p2p_server_version_reject_test.cpp)
target_include_directories(caesar_p2p_server_version_reject_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(caesar_p2p_server_version_reject_test PRIVATE crypto pthread)
add_test(NAME caesar_p2p_server_version_reject_test COMMAND caesar_p2p_server_version_reject_test)

add_executable(caesar_blockchain_storage_test
    tests/blockchain_storage_test.cpp
)
target_include_directories(
    caesar_blockchain_storage_test
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(
    caesar_blockchain_storage_test
    PRIVATE
    OpenSSL::Crypto
)
add_test(
    NAME CaesarBlockchainStorageTest
    COMMAND caesar_blockchain_storage_test
)

add_executable(caesar_p2p_frame_security_test
    tests/p2p_frame_security_test.cpp
)

target_include_directories(caesar_p2p_frame_security_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_frame_security_test
    PRIVATE
        OpenSSL::Crypto
        pthread
)

add_test(
    NAME caesar_p2p_frame_security_test
    COMMAND caesar_p2p_frame_security_test
)

add_executable(caesar_p2p_connection_security_test
    tests/p2p_connection_security_test.cpp
)

target_include_directories(caesar_p2p_connection_security_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_connection_security_test
    PRIVATE
        OpenSSL::Crypto
        pthread
)

add_test(
    NAME caesar_p2p_connection_security_test
    COMMAND caesar_p2p_connection_security_test
)

add_executable(caesar_node_test tests/node_test.cpp)
target_include_directories(caesar_node_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_node_test PRIVATE crypto pthread)
add_test(NAME CaesarNodeTest COMMAND caesar_node_test)

add_executable(caesar_two_node_p2p_test
    tests/two_node_p2p_test.cpp
)
target_include_directories(caesar_two_node_p2p_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_two_node_p2p_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarTwoNodeP2PTest
    COMMAND caesar_two_node_p2p_test
)


add_executable(caesar_chain_sync_test
    tests/chain_sync_test.cpp
)

target_include_directories(caesar_chain_sync_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_chain_sync_test PRIVATE
    crypto
    pthread
)

add_test(
    NAME CaesarChainSyncTest
    COMMAND caesar_chain_sync_test
)


add_executable(caesar_chain_work_test
    tests/chain_work_test.cpp
)
target_include_directories(caesar_chain_work_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_chain_work_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarChainWorkTest
    COMMAND caesar_chain_work_test
)

add_executable(caesar_dynamic_mining_difficulty_test
    tests/dynamic_mining_difficulty_test.cpp
)
target_include_directories(caesar_dynamic_mining_difficulty_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_dynamic_mining_difficulty_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarDynamicMiningDifficultyTest
    COMMAND caesar_dynamic_mining_difficulty_test
)

add_executable(caesar_storage_utxo_rejection_test
    tests/storage_utxo_rejection_test.cpp
)
target_include_directories(caesar_storage_utxo_rejection_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_storage_utxo_rejection_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarStorageUtxoRejectionTest
    COMMAND caesar_storage_utxo_rejection_test
)

add_executable(caesar_headers_size_regression_test
    tests/headers_size_regression_test.cpp
)
target_include_directories(caesar_headers_size_regression_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_headers_size_regression_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarHeadersSizeRegressionTest
    COMMAND caesar_headers_size_regression_test
)

add_executable(caesar_p2p_lifecycle_race_test
    tests/p2p_lifecycle_race_test.cpp
)
target_include_directories(caesar_p2p_lifecycle_race_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_p2p_lifecycle_race_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarP2PLifecycleRaceTest
    COMMAND caesar_p2p_lifecycle_race_test
)

add_executable(caesar_p2p_message_type_range_test
    tests/p2p_message_type_range_test.cpp
)
target_include_directories(caesar_p2p_message_type_range_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_p2p_message_type_range_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarP2PMessageTypeRangeTest
    COMMAND caesar_p2p_message_type_range_test
)

add_executable(caesar_genesis_canonical_test
    tests/genesis_canonical_test.cpp
)
target_include_directories(caesar_genesis_canonical_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_genesis_canonical_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarGenesisCanonicalTest
    COMMAND caesar_genesis_canonical_test
)

add_executable(caesar_timestamp_consensus_test
    tests/timestamp_consensus_test.cpp
)
target_include_directories(caesar_timestamp_consensus_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_timestamp_consensus_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarTimestampConsensusTest
    COMMAND caesar_timestamp_consensus_test
)

add_executable(caesar_storage_durability_test
    tests/storage_durability_test.cpp
)
target_include_directories(caesar_storage_durability_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_storage_durability_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarStorageDurabilityTest
    COMMAND caesar_storage_durability_test
)

add_executable(caesar_mempool_reorg_test
    tests/mempool_reorg_test.cpp
)
target_include_directories(caesar_mempool_reorg_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_mempool_reorg_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarMempoolReorgTest
    COMMAND caesar_mempool_reorg_test
)

add_executable(caesar_reorg_work_selection_test
    tests/reorg_work_selection_test.cpp
)
target_include_directories(caesar_reorg_work_selection_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_reorg_work_selection_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarReorgWorkSelectionTest
    COMMAND caesar_reorg_work_selection_test
)

add_executable(caesar_node_network_identity_test
    tests/node_network_identity_test.cpp
)
target_include_directories(caesar_node_network_identity_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_node_network_identity_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarNodeNetworkIdentityTest
    COMMAND caesar_node_network_identity_test
)

add_executable(caesar_p2p_peer_scoring_test
    tests/p2p_peer_scoring_test.cpp
)
target_include_directories(caesar_p2p_peer_scoring_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_p2p_peer_scoring_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarP2PPeerScoringTest
    COMMAND caesar_p2p_peer_scoring_test
)

add_executable(caesar_chain_replacement_test
    tests/chain_replacement_test.cpp
)
target_include_directories(caesar_chain_replacement_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_chain_replacement_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarChainReplacementTest
    COMMAND caesar_chain_replacement_test
)

add_executable(caesar_chain_replacement_prepare_test
    tests/chain_replacement_prepare_test.cpp
)
target_include_directories(caesar_chain_replacement_prepare_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_chain_replacement_prepare_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarChainReplacementPrepareTest
    COMMAND caesar_chain_replacement_prepare_test
)

add_executable(caesar_candidate_chain_validation_test
    tests/candidate_chain_validation_test.cpp
)

target_include_directories(caesar_candidate_chain_validation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_candidate_chain_validation_test PRIVATE
    crypto
    pthread
)

add_test(
    NAME CaesarCandidateChainValidationTest
    COMMAND caesar_candidate_chain_validation_test
)

add_executable(caesar_node_chain_replacement_test
    tests/node_chain_replacement_test.cpp
)
target_include_directories(caesar_node_chain_replacement_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_node_chain_replacement_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarNodeChainReplacementTest
    COMMAND caesar_node_chain_replacement_test
)

add_executable(caesar_chain_candidate_assembly_test
    tests/chain_candidate_assembly_test.cpp
)
target_include_directories(caesar_chain_candidate_assembly_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_chain_candidate_assembly_test
    PRIVATE crypto pthread
)
add_test(
    NAME CaesarChainCandidateAssemblyTest
    COMMAND caesar_chain_candidate_assembly_test
)

add_executable(caesar_p2p_fork_reorg_test
    tests/p2p_fork_reorg_test.cpp
)
target_include_directories(caesar_p2p_fork_reorg_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_p2p_fork_reorg_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarP2PForkReorgTest
    COMMAND caesar_p2p_fork_reorg_test
)

add_executable(caesar_chain_replacement_rejection_test
    tests/chain_replacement_rejection_test.cpp
)
target_include_directories(caesar_chain_replacement_rejection_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_chain_replacement_rejection_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarChainReplacementRejectionTest
    COMMAND caesar_chain_replacement_rejection_test
)

add_executable(caesar_p2p_sync_protocol_test
    tests/p2p_sync_protocol_test.cpp
)
target_include_directories(caesar_p2p_sync_protocol_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_p2p_sync_protocol_test PRIVATE
    crypto
    pthread
)
add_test(
    NAME CaesarP2PSyncProtocolTest
    COMMAND caesar_p2p_sync_protocol_test
)

add_executable(caesar_transaction_relay_test
    tests/transaction_relay_test.cpp
)

target_include_directories(caesar_transaction_relay_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_transaction_relay_test PRIVATE
    crypto
    pthread
)

add_test(
    NAME CaesarTransactionRelayTest
    COMMAND caesar_transaction_relay_test
)

# ---------------------------------------------------------------------------
# Force asserts active in every test target, even in Release.
#
# CMake's Release config appends -DNDEBUG, which turns every assert()
# into a no-op. Tests that rely on assert() then silently pass without
# checking anything. We defeat NDEBUG by adding -UNDEBUG after the
# Release flags (the last flag wins) and force-include
# tests/test_assertions.hpp, which trips a #error if NDEBUG is still
# defined when a test is compiled.
#
# This makes the failure mode loud instead of silent: if a future
# refactor drops -UNDEBUG, the test build fails at compile time with
# a clear diagnostic instead of letting the tests pass vacuously.
#
# Production targets (caesard, etc.) are not affected and continue to
# build with -DNDEBUG for speed.
# ---------------------------------------------------------------------------
get_property(CAESAR_ALL_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)

foreach(_t ${CAESAR_ALL_TARGETS})
    if(_t MATCHES "^caesar_.*_test$")
        target_compile_options(${_t} PRIVATE -UNDEBUG)
        target_compile_options(${_t} PRIVATE
            -include ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_assertions.hpp)
    endif()
endforeach()

unset(CAESAR_ALL_TARGETS)
unset(_t)
