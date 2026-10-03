cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

add_compile_definitions(CPPHTTPLIB_NO_IFADDRS=1)

add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")

target_include_directories(caesar_web PRIVATE
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/third_party"
    "${OPENSSL_ROOT}/include"
)

find_library(CRYPTO_LIB crypto PATHS "${OPENSSL_ROOT}/lib" NO_DEFAULT_PATH)

if(NOT CRYPTO_LIB)
    set(CRYPTO_LIB "${OPENSSL_ROOT}/lib/libcrypto.a")
endif()

target_link_libraries(caesar_web PRIVATE
    "${CRYPTO_LIB}"
    z
    log
    android
    dl
)

target_link_options(caesar_web PRIVATE
    "-Wl,--gc-sections"
    "-Wl,--exclude-libs=ALL"
    "-Wl,-z,max-page-size=16384"
)
