cmake_minimum_required(VERSION 3.20)

project(CaesarCZR VERSION 0.1.0 LANGUAGES CXX)

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

add_executable(caesar_blockchain_sync_test
    tests/blockchain_sync_test.cpp
)

target_include_directories(caesar_blockchain_sync_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(caesar_blockchain_sync_test PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME CaesarBlockchainSyncTest
    COMMAND caesar_blockchain_sync_test
)

add_executable(caesar_p2p_reorg_test
    tests/p2p_reorg_test.cpp
)

target_include_directories(caesar_p2p_reorg_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_reorg_test PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME CaesarP2PReorgTest
    COMMAND caesar_p2p_reorg_test
)

add_executable(caesar_p2p_relay_headers_validation_test
    tests/p2p_relay_headers_validation_test.cpp
)

target_include_directories(caesar_p2p_relay_headers_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_relay_headers_validation_test PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME CaesarP2PRelayHeadersValidationTest
    COMMAND caesar_p2p_relay_headers_validation_test
)

add_executable(caesar_p2p_relay_disconnected_block_test
    tests/p2p_relay_disconnected_block_test.cpp
)

target_include_directories(caesar_p2p_relay_disconnected_block_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(caesar_p2p_relay_disconnected_block_test PRIVATE
    OpenSSL::Crypto
)

add_test(
    NAME CaesarP2PRelayDisconnectedBlockTest
    COMMAND caesar_p2p_relay_disconnected_block_test
)

add_executable(caesar_carr_core_test
    tools/caesar-doctor/carr_test.cpp
    tools/caesar-doctor/carr_types.cpp
    tools/caesar-doctor/carr_diagnosis.cpp
    tools/caesar-doctor/carr_policy.cpp
    tools/caesar-doctor/carr_repair.cpp
    tools/caesar-doctor/carr_journal.cpp
    tools/caesar-doctor/carr_engine.cpp
)

target_include_directories(caesar_carr_core_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/caesar-doctor
)

add_test(
    NAME CaesarCARRCoreTest
    COMMAND caesar_carr_core_test
)

set_tests_properties(
    CaesarCARRCoreTest
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "CAESAR_CARR_TEST_ROOT=${CMAKE_BINARY_DIR}/carr-test-runtime"
)
add_executable(caesar_carr_runtime_test
    tools/caesar-doctor/carr_runtime_test.cpp
    tools/caesar-doctor/carr_runtime.cpp
    tools/caesar-doctor/carr_supervisor.cpp
    tools/caesar-doctor/carr_engine.cpp
    tools/caesar-doctor/carr_diagnosis.cpp
    tools/caesar-doctor/carr_policy.cpp
    tools/caesar-doctor/carr_repair.cpp
    tools/caesar-doctor/carr_journal.cpp
    tools/caesar-doctor/carr_types.cpp
)

target_include_directories(caesar_carr_runtime_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/caesar-doctor
)

add_test(
    NAME CaesarCARRRuntimeTest
    COMMAND caesar_carr_runtime_test
)

set_tests_properties(
    CaesarCARRRuntimeTest
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "CAESAR_CARR_RUNTIME_TEST_ROOT=${CMAKE_BINARY_DIR}/carr-runtime-test"
)

add_executable(caesar_carr_supervisor_test
    tools/caesar-doctor/carr_supervisor_test.cpp
    tools/caesar-doctor/carr_supervisor.cpp
    tools/caesar-doctor/carr_runtime.cpp
    tools/caesar-doctor/carr_engine.cpp
    tools/caesar-doctor/carr_diagnosis.cpp
    tools/caesar-doctor/carr_policy.cpp
    tools/caesar-doctor/carr_repair.cpp
    tools/caesar-doctor/carr_journal.cpp
    tools/caesar-doctor/carr_types.cpp
)

target_include_directories(caesar_carr_supervisor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/caesar-doctor
)

add_test(
    NAME CaesarCARRSupervisorTest
    COMMAND caesar_carr_supervisor_test
)

set_tests_properties(
    CaesarCARRSupervisorTest
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(caesar_carr_caesard_integration_test
    tools/caesar-doctor/carr_caesard_integration_test.cpp
    tools/caesar-doctor/carr_supervisor.cpp
)

target_include_directories(caesar_carr_caesard_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/caesar-doctor
)

add_dependencies(
    caesar_carr_caesard_integration_test
    caesard
)

add_test(
    NAME CaesarCARRCaesardIntegrationTest
    COMMAND caesar_carr_caesard_integration_test
)

set_tests_properties(
    CaesarCARRCaesardIntegrationTest
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ENVIRONMENT "CAESARD_EXECUTABLE=$<TARGET_FILE:caesard>;CAESAR_CARR_CAESARD_TEST_ROOT=${CMAKE_BINARY_DIR}/carr-caesard-runtime"
)


add_executable(caesar_fee_policy_test
    tests/fee_policy_test.cpp
    src/fee_policy.cpp
)

target_include_directories(caesar_fee_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME CaesarFeePolicyTest
    COMMAND caesar_fee_policy_test
)

set_tests_properties(
    CaesarFeePolicyTest
    PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(caesar_economics_test
    tests/economics_test.cpp
    src/economics.cpp
)

target_include_directories(caesar_economics_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME CaesarEconomicsTest
    COMMAND caesar_economics_test
)

set_tests_properties(
    CaesarEconomicsTest
    PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(caesar_p2p_reorg_fixture
    tests/p2p_reorg_fixture.cpp
)
target_include_directories(caesar_p2p_reorg_fixture PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(caesar_p2p_reorg_fixture PRIVATE
    OpenSSL::Crypto
)
