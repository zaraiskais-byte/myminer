import sys
import os
import socket

CONFIG_PATH = os.path.expanduser("~/caesar-czr-internal-real/config/czr_master.conf")
LOG_PATH = os.path.expanduser("~/caesar-czr-internal-real/logs/sentinel.log")

def log(message):
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "a") as f:
        f.write(f"[SENTINEL] {message}\n")
    print(f"[SENTINEL] {message}")

def verify_ports():
    ports = [38543, 39555]
    for port in ports:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(1.0)
            result = s.connect_ex(('127.0.0.1', port))
            if result == 0:
                log(f"Port {port} is active and listening.")
            else:
                log(f"Port {port} is currently closed or unassigned.")

if __name__ == "__main__":
    log("Initializing Caesar CZR Security & Node Sentinel...")
    verify_ports()
    log("Sentinel verification routine completed successfully.")
