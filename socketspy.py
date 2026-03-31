import os
import time
import socket
import struct

INTERVAL = 1.0

def hex_to_ip_port(hex_addr):
    ip_hex, port_hex = hex_addr.split(':')
    ip = socket.inet_ntoa(struct.pack("<L", int(ip_hex, 16)))
    port = int(port_hex, 16)
    return ip, port

def parse_net_file(path):
    conns = set()
    try:
        with open(path, "r") as f:
            lines = f.readlines()[1:]
            for line in lines:
                parts = line.split()
                remote = parts[2]

                r_ip, r_port = hex_to_ip_port(remote)
                conns.add((r_ip, r_port))
    except Exception:
        pass
    return conns

def get_pid_conns(pid):
    base = f"/proc/{pid}/net"
    conns = set()
    for proto in ["tcp", "tcp6", "udp", "udp6"]:
        path = os.path.join(base, proto)
        conns |= parse_net_file(path)
    return conns

def get_cmdline(pid):
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            data = f.read().replace(b'\x00', b' ').decode(errors='ignore')
            return data.strip() if data else "[kernel/empty]"
    except Exception:
        return "[unknown]"

def main():
    pid = input("PID: ").strip()

    if not pid.isdigit() or not os.path.exists(f"/proc/{pid}"):
        print("Invalid PID")
        return

    prev = set()

    while True:
        if not os.path.exists(f"/proc/{pid}"):
            print(f"PID {pid} exited")
            break

        current = get_pid_conns(pid)
        new = current - prev

        if new:
            cmd = get_cmdline(pid)
            for ip, port in new:
                print(f"[+] PID={pid} CMD='{cmd}' -> {ip}:{port}")

        prev = current
        time.sleep(INTERVAL)

if __name__ == "__main__":
    main()
