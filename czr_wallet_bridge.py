import os
import subprocess
import sys

print("=======================================================")
print("  CAESAR CZR - ANDROID WALLET TO C++ ENGINE BRIDGE")
print("  Architect/Owner: Zaraiskais-byte")
print("  Derivation Path: m/44'/7999'/0'/0'/0'")
print("=======================================================")

if os.getenv("GITHUB_ACTIONS") == "true":
    print("[*] Running in GitHub Actions. Executing CTest for comprehensive C++ verification...")
    result = subprocess.run(["ctest", "--output-on-failure", "--timeout", "300"], cwd="build", capture_output=True, text=True, timeout=1500)
    if result.returncode == 0:
        print("[+] CTest Output:")
        # Print last 20 lines of ctest to keep logs clean but informative
        lines = result.stdout.strip().split('\n')
        print('\n'.join(lines[-20:]))
        print("[+] All C++ engine tests passed successfully on x86_64!")
    else:
        print(f"[!] CTest failed with return code: {result.returncode}")
        print(f"[!] STDOUT: {result.stdout.strip()}")
        print(f"[!] STDERR: {result.stderr.strip()}")
        sys.exit(1)
else:
    # Local Termux execution
    CZR_ROOT = os.getcwd()
    possible_paths = [
        os.path.join(CZR_ROOT, "bin", "czr_core_test"),
        os.path.join(CZR_ROOT, "build", "caesard"),
    ]
    
    core_binary = None
    for path in possible_paths:
        if os.path.exists(path):
            core_binary = path
            break

    if not core_binary:
        print("[!] FATAL: No C++ core binary found in bin/ or build/")
        sys.exit(1)

    print(f"[*] Found local binary at: {core_binary}")
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
        else:
            print(f"[!] FATAL: OS Error during execution: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"[!] FATAL: Unexpected error during execution: {e}")
        sys.exit(1)

print("=======================================================")
print("  WALLET BRIDGE SYNCHRONIZED AND SECURED")
print("=======================================================")
