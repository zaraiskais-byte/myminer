import os
import subprocess
import sys

CZR_ROOT = os.path.expanduser("~/caesar-czr-internal-real")

def run_command(desc, cmd):
    print(f"[*] {desc}...")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[!] Error in {desc}: {result.stderr.strip()}")
        sys.exit(1)
    print(f"[+] {desc} passed successfully.")

if __name__ == "__main__":
    print("=======================================================")
    print("  RUNNING CAESAR CZR MASTER SYSTEM INTEGRATION")
    print("  Architect: Zaraiskais-byte")
    print("=======================================================")
    
    run_command("Node Sentinel Health Check", f"python3 {CZR_ROOT}/czr_sentinel.py")
    run_command("Core C++ Engine Execution", f"{CZR_ROOT}/bin/czr_core_test")
    run_command("Wallet Cryptographic Signer", f"python3 {CZR_ROOT}/czr_wallet_signer.py")
    
    print("=======================================================")
    print("  ALL SYSTEMS FULLY OPERATIONAL AND VERIFIED!")
    print("  Project: Caesar CZR | Owner: Zaraiskais-byte")
    print("=======================================================")
