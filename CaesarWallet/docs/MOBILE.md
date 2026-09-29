# Mobile Build Guide (Termux & Android)
## Requirements
- Android 8.0+
- Termux (F-Droid version)
- 4GB RAM minimum
## Steps
1. pkg install clang cmake openssl
2. cmake -S . -B build -DCAESAR_ANDROID=ON
3. cmake --build build -j2
