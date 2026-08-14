import sys
import time
import datetime
import ctypes
from ctypes import wintypes
import psutil
import struct

# Ensure UTF-8 output encoding for Windows terminal
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

# Windows API Constants & Structs
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
MEM_COMMIT = 0x1000
PAGE_READWRITE = 0x04
PAGE_READONLY = 0x02
PAGE_EXECUTE_READWRITE = 0x40
PAGE_EXECUTE_READ = 0x20

class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wintypes.DWORD),
        ("PartitionId", wintypes.WORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", wintypes.DWORD),
        ("Protect", wintypes.DWORD),
        ("Type", wintypes.DWORD),
    ]

kernel32 = ctypes.windll.kernel32

OpenProcess = kernel32.OpenProcess
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
OpenProcess.restype = wintypes.HANDLE

ReadProcessMemory = kernel32.ReadProcessMemory
ReadProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPCVOID, wintypes.LPVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
ReadProcessMemory.restype = wintypes.BOOL

VirtualQueryEx = kernel32.VirtualQueryEx
VirtualQueryEx.argtypes = [wintypes.HANDLE, wintypes.LPCVOID, ctypes.POINTER(MEMORY_BASIC_INFORMATION), ctypes.c_size_t]
VirtualQueryEx.restype = ctypes.c_size_t

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

def find_mumu_process():
    for proc in psutil.process_iter(['pid', 'name']):
        try:
            name = proc.info['name']
            if name and name.lower() in ['mumunxdevice.exe', 'mumuvmmheadless.exe', 'nemuheadless.exe']:
                return proc.info['pid'], name
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass
    return None, None

def scan_active_zones_in_ram(h_process):
    mbi = MEMORY_BASIC_INFORMATION()
    address = 0
    max_address = 0x7FFFFFFFFFFF
    pattern = b"DinoSpawnEntries_"
    
    zone_candidates = {}
    valid_protections = (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ)

    while address < max_address:
        res = VirtualQueryEx(h_process, ctypes.c_void_p(address), ctypes.byref(mbi), ctypes.sizeof(mbi))
        if res == 0:
            break
            
        if mbi.State == MEM_COMMIT and (mbi.Protect & valid_protections):
            size = mbi.RegionSize
            if size <= 50 * 1024 * 1024:
                buffer = ctypes.create_string_buffer(size)
                bytes_read = ctypes.c_size_t(0)
                if ReadProcessMemory(h_process, ctypes.c_void_p(address), buffer, size, ctypes.byref(bytes_read)):
                    raw_data = buffer.raw[:bytes_read.value]
                    idx = 0
                    while True:
                        idx = raw_data.find(pattern, idx)
                        if idx == -1:
                            break
                        end_idx = raw_data.find(b'\x00', idx)
                        if end_idx != -1 and end_idx - idx < 100:
                            try:
                                zone_str = raw_data[idx:end_idx].decode('ascii', errors='ignore')
                                if len(zone_str) > 17 and "_" in zone_str[17:]:
                                    # Filter out static base strings, keep active sub-zones (e.g. DinoSpawnEntries_Beach_South_C)
                                    zone_candidates[zone_str] = address + idx
                            except:
                                pass
                        idx += len(pattern)
        address += mbi.RegionSize

    return zone_candidates

def main():
    print("=" * 75)
    print("  TOOL CHUẨN HOÁ DUMP KHU VỰC UNREAL ENGINE (MUMU PLAYER)")
    print("=" * 75)
    
    pid, proc_name = find_mumu_process()
    if not pid:
        print("[-] KHONG TIM THAY MUMU PLAYER!")
        return

    print(f"[+] Tien trinh: {proc_name} (PID: {pid})")
    
    h_process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not h_process:
        print("[-] LOI QUYEN TRUY CAP RAM! Vui long chay CMD bang quyen Administrator.")
        return

    print("\n[*] DANG QUET FILTER CHUAN XAC CAC KHU VUC SPAWN TRONG RAM...")
    print("    (Tự động lọc chính xác tên Khu vực cụ thể - Cập nhật 5s/lần)")
    print("=" * 75)

    scan_count = 0
    try:
        while True:
            scan_count += 1
            now_str = datetime.datetime.now().strftime("%H:%M:%S")
            
            zones_dict = scan_active_zones_in_ram(h_process)
            
            print(f"\n--- [CẬP NHẬT TRẠNG THÁI #{scan_count} - {now_str}] ---")
            if zones_dict:
                print(f"🗺️  DANH SÁCH KHU VỰC CỤ THỂ ĐANG NẠP Ở VỊ TRÍ HIỆN TẠI ({len(zones_dict)} Zone):")
                for z_name, z_addr in sorted(zones_dict.items()):
                    print(f"    >>> KHU VỰC: {z_name:<35} | (Địa chỉ RAM: 0x{z_addr:X})")
            else:
                print("🗺️  Đang phân tích bộ nhớ RAM...")

            time.sleep(5)
    except KeyboardInterrupt:
        print("\n[*] Da dung tool.")
    finally:
        CloseHandle(h_process)

if __name__ == "__main__":
    main()
