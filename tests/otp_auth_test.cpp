#include <cassert>
#include <filesystem>
#include <iostream>
#include "caesar/otp_auth.hpp"

int main() {
    using namespace caesar;
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "caesar_otp_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    OtpStore store(tmp / "otp.txt");

    std::string code = store.create("user@example.com");
    assert(code.size() == 6);
    for (char c : code) assert(c >= '0' && c <= '9');
    std::cout << "[test] code generated: " << code << "\n";

    assert(store.verify("user@example.com", code));
    std::cout << "[test] correct code accepted\n";

    assert(!store.verify("user@example.com", code));
    std::cout << "[test] reused code rejected\n";

    assert(!store.verify("other@example.com", "000000"));
    std::cout << "[test] unknown identifier rejected\n";

    std::string code2 = store.create("b@example.com");
    std::string wrong = (code2 == "000000") ? "111111" : "000000";
    assert(!store.verify("b@example.com", wrong));
    std::cout << "[test] wrong code rejected\n";

    assert(store.verify("b@example.com", code2));
    std::cout << "[test] correct code accepted after wrong attempt\n";

    OtpStore store2(tmp / "otp.txt");
    assert(!store2.verify("b@example.com", code2));
    std::cout << "[test] persistence reload works\n";

    std::filesystem::remove_all(tmp);
    std::cout << "[test] OtpAuthTest passed successfully.\n";
    return 0;
}
