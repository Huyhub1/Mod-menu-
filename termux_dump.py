import os
import sys
import time

def get_ark_pid():
    # Finding ARK process PID on Android
    pid = os.popen("su -c 'pidof com.studiowildcard.wardrumstudios.ark'").read().strip()
    if not pid:
        # Fallback search ps
        pid = os.popen("su -c 'ps -ef | grep -i wardrumstudios | grep -v grep | awk \"{print $2}\"'").read().strip()
    return pid

def dump_zones_from_mem(pid):
    cmd = f"su -c 'grep -a -o \"DinoSpawnEntries_[A-Za-z0-9_]*\" /proc/{pid}/mem | sort -u'"
    raw_output = os.popen(cmd).read()
    zones = set()
    for line in raw_output.splitlines():
        z = line.strip()
        if len(z) > 17 and "_" in z[17:]:
            zones.add(z)
    return zones

def main():
    print("=" * 60)
    print("      TERMUX ROOT ZONE DUMPER - ARK MOBILE (ANDROID)")
    print("=" * 60)
    
    pid = get_ark_pid()
    if not pid:
        print("[-] KHÔNG TÌM THẤY GAME ARK ĐANG CHẠY!")
        print("[-] Hãy mở game ARK Mobile trong MuMu trước khi chạy Termux.")
        return

    print(f"[+] Đã tìm thấy tiến trình ARK PID: {pid}")
    print("[*] Đang tự động quét RAM trong Android (Cập nhật 3s/lần)...")
    print("-" * 60)

    last_zones = set()
    while True:
        zones = dump_zones_from_mem(pid)
        if zones and zones != last_zones:
            print(f"\n[📍] KHU VỰC HIỆN TẠI NẠP TRONG RAM ({time.strftime('%H:%M:%S')}):")
            for z in sorted(zones):
                print(f"    >>> {z}")
            last_zones = zones
        time.sleep(3)

if __name__ == "__main__":
    main()
