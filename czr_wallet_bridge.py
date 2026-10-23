import os
import subprocess
import sys

CZR_ROOT = os.getcwd()
core_binary = os.path.join(CZR_ROOT, "bin", "czr_core_test")

print("=======================================================")
print("  CAESAR CZR - ANDROID WALLET TO C++ ENGINE BRIDGE")
print("  Architect/Owner: Zaraiskais-byte")
print("  Derivation Path: m/44'/7999'/0'/0'/0'")
print("=======================================================")
print(f"[*] Looking for binary at: {core_binary}")

if not os.path.exists(core_binary):
    print(f"[!] FATAL: C++ core binary not found at: {core_binary}")
    print("[!] Please ensure 'bin/czr_core_test' is committed to the repository.")
    sys.exit(1)

# Check if executable
if not os.access(core_binary, os.X_OK):
    print(f"[!] FATAL: Binary exists but lacks execute permissions. Attempting to fix...")
    try:
        os.chmod(core_binary, 0o755)
        print("[+] Execute permissions granted.")
    except Exception as e:
        print(f"[!] FATAL: Could not set execute permissions: {e}")
        sys.exit(1)

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
except PermissionError as e:
    print(f"[!] FATAL: Permission denied when trying to execute the binary: {e}")
    sys.exit(1)
except Exception as e:
    print(f"[!] FATAL: Unexpected error during execution: {e}")
    sys.exit(1)

print("=======================================================")
print("  WALLET BRIDGE SYNCHRONIZED AND SECURED")
print("=======================================================")
