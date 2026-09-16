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
