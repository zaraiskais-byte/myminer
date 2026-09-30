cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")
target_include_directories(caesar_web PRIVATE
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/third_party"
    "${OPENSSL_ROOT}/include")
target_link_libraries(caesar_web PRIVATE
    "${OPENSSL_ROOT}/lib/libcrypto.a"
    log)
target_link_options(caesar_web PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
