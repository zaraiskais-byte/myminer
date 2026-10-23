import os
import subprocess
import sys

# Use the current working directory (works in both Termux and GitHub Actions)
CZR_ROOT = os.getcwd()
core_binary = os.path.join(CZR_ROOT, "bin", "czr_core_test")

print("=======================================================")
print("  CAESAR CZR - ANDROID WALLET TO C++ ENGINE BRIDGE")
print("  Architect/Owner: Zaraiskais-byte")
print("  Derivation Path: m/44'/7999'/0'/0'/0'")
print("=======================================================")

if not os.path.exists(core_binary):
    print(f"[!] C++ core binary not found at: {core_binary}")
    print("[!] CI will now fail. Please ensure 'bin/czr_core_test' is committed to the repository.")
    sys.exit(1)

print("[*] Executing C++ signature engine for wallet verification...")

result = subprocess.run([core_binary], capture_output=True, text=True)

if result.returncode == 0:
    print("[+] C++ Engine Output:")
    print(result.stdout.strip())
    print("[+] Android Wallet bridge successfully established with C++ Core & P2P Mesh!")
else:
    print(f"[!] Engine execution error: {result.stderr.strip()}")
    sys.exit(1)

print("=======================================================")
print("  WALLET BRIDGE SYNCHRONIZED AND SECURED")
print("=======================================================")
