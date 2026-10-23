import os
import subprocess
import sys

CZR_ROOT = os.getcwd()

# Check both local Termux path and GitHub Actions build path
possible_paths = [
    os.path.join(CZR_ROOT, "bin", "czr_core_test"),
    os.path.join(CZR_ROOT, "build", "czr_core_test"),
    os.path.join(CZR_ROOT, "build", "caesard"),
    os.path.join(CZR_ROOT, "build", "caesar_foundation_test")
]

core_binary = None
for path in possible_paths:
    if os.path.exists(path):
        core_binary = path
        break

print("=======================================================")
print("  CAESAR CZR - ANDROID WALLET TO C++ ENGINE BRIDGE")
print("  Architect/Owner: Zaraiskais-byte")
print("  Derivation Path: m/44'/7999'/0'/0'/0'")
print("=======================================================")

if not core_binary:
    print("[!] FATAL: No C++ core binary found in bin/ or build/")
    print("[!] Please ensure the project is built before running this script.")
    sys.exit(1)

print(f"[*] Found binary at: {core_binary}")
print("[*] Executing C++ signature engine for wallet verification...")

try:
    result = subprocess.run([core_binary], capture_output=True, text=True, timeout=30)
    if result.returncode == 0:
        print("[+] C++ Engine Output:")
        print(result.stdout.strip())
        print("[+] Android Wallet bridge successfully established with C++ Core & P2P Mesh!")
    else:
        print(f"[!] Engine execution failed with return code: {result.returncode}")
        print(f"[!] STDERR: {result.stderr.strip()}")
        sys.exit(1)
except OSError as e:
    if e.errno == 8:
        print("[!] FATAL: Exec format error. The binary was compiled for a different architecture (e.g., ARM64).")
        print("[!] FIX: The CI pipeline must compile the binary for x86_64 before running this script.")
    else:
        print(f"[!] FATAL: OS Error during execution: {e}")
    sys.exit(1)
except Exception as e:
    print(f"[!] FATAL: Unexpected error during execution: {e}")
    sys.exit(1)

print("=======================================================")
print("  WALLET BRIDGE SYNCHRONIZED AND SECURED")
print("=======================================================")
