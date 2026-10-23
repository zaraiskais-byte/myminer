#include <iostream>
#include <string>
#include <vector>

class CaesarCoreEngine {
public:
    std::string architect = "Zaraiskais-byte";
    int blockTarget = 120;
    double poolFee = 2.0;

    void status() {
        std::cout << "[CORE] Caesar CZR Engine loaded successfully.\n";
        std::cout << "[CORE] Architect: " << architect << "\n";
        std::cout << "[CORE] Target Block Time: " << blockTarget << "s\n";
        std::cout << "[CORE] Mining Pool Fee: " << poolFee << "%\n";
    }
};

int main() {
    CaesarCoreEngine engine;
    engine.status();
    return 0;
}
