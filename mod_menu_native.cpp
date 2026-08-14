#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>

// ================================================================
// ARK MOBILE NATIVE C++ ENGINE v3.0 (Termux / Root Binary)
// Author: GoGs Ultimate GSV  |  Build: 2026-08-14
// Features: Interactive CLI, Fast RAM Scan/Patch, Teleport, Watcher
// ================================================================

// ANSI Colors for Terminal
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"

struct ZoneEntry {
    std::string name;
    unsigned long long address;
};

struct CreatureEntry {
    std::string type;
    unsigned long long address;
};

struct PlayerXYZ {
    unsigned long long addrX = 0, addrY = 0, addrZ = 0;
    float valX = 0.0f, valY = 0.0f, valZ = 0.0f;
};

// Global Session State
static std::vector<ZoneEntry> g_sessionZones;
static std::vector<CreatureEntry> g_sessionCreatures;
static PlayerXYZ g_sessionPlayer;
static std::string g_targetFilter = "";
static pid_t g_currentPid = -1;

// Helper: Get Current Time String
static std::string get_current_time() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", t);
    return std::string(buf);
}

// Helper: Format Hex Address
static std::string fmt_hex(unsigned long long addr) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::hex << addr;
    return ss.str();
}

// Helper: Case-insensitive String Contains
static bool contains_ignore_case(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return (it != str.end());
}

// ================================================================
// PID FINDER ENGINE
// ================================================================

pid_t find_ark_pid() {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* ptr;
    pid_t found_pid = -1;

    while ((ptr = readdir(dir)) != NULL) {
        if (ptr->d_type == DT_DIR) {
            pid_t p = atoi(ptr->d_name);
            if (p > 0) {
                char cmdpath[128];
                snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", p);
                int fd = open(cmdpath, O_RDONLY);
                if (fd >= 0) {
                    char cmdline[256] = {0};
                    ssize_t bytes = read(fd, cmdline, sizeof(cmdline) - 1);
                    close(fd);
                    if (bytes > 0) {
                        if (strstr(cmdline, "studiowildcard") ||
                            strstr(cmdline, "wardrumstudios") ||
                            strstr(cmdline, "ark")) {
                            found_pid = p;
                            break;
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
    return found_pid;
}

// ================================================================
// MEMORY READ / WRITE HELPERS
// ================================================================

bool write_float_to_ram(pid_t pid, unsigned long long address, float value) {
    char mem_path[128];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int fd = open(mem_path, O_RDWR);
    if (fd < 0) return false;

    ssize_t ret = pwrite(fd, &value, sizeof(float), (off_t)address);
    close(fd);
    return (ret == sizeof(float));
}

bool read_memory_bytes(pid_t pid, unsigned long long address, void* buffer, size_t size) {
    char mem_path[128];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int fd = open(mem_path, O_RDONLY);
    if (fd < 0) return false;

    ssize_t ret = pread(fd, buffer, size, (off_t)address);
    close(fd);
    return (ret == (ssize_t)size);
}

// ================================================================
// MODULE 1: SPAWN ZONE DUMPER
// ================================================================

void dump_zones(pid_t pid, const std::string& filterStr) {
    std::cout << COLOR_CYAN << "\n==================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "[📍] DUMP SPAWN ZONES (DinoSpawnEntries_...)" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "PID: " << pid << " | Filter: \"" << (filterStr.empty() ? "All" : filterStr) << "\"" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) {
        std::cout << COLOR_RED << "[-] Khong mo duoc /proc/" << pid << "/maps!" << COLOR_RESET << std::endl;
        return;
    }
    int mem_fd = open(mem_path, O_RDONLY);
    if (mem_fd < 0) {
        std::cout << COLOR_RED << "[-] Khong mo duoc /proc/" << pid << "/mem! (Can Root 'su')" << COLOR_RESET << std::endl;
        fclose(maps);
        return;
    }

    g_sessionZones.clear();
    std::set<std::string> seenNames;
    char line[512];
    int matchCount = 0;

    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "rw-p") || strstr(line, "r--p")) {
            unsigned long long start, end;
            if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
                size_t size = end - start;
                if (size > 0 && size <= 25 * 1024 * 1024) { // Max 25MB block
                    char* buf = (char*)malloc(size);
                    if (buf) {
                        if (pread(mem_fd, buf, size, (off_t)start) > 0) {
                            char* ptr = buf;
                            char* end_ptr = buf + size - 17;
                            while (ptr < end_ptr) {
                                if (memcmp(ptr, "DinoSpawnEntries_", 17) == 0) {
                                    char zoneBuf[128] = {0};
                                    int k = 0;
                                    while (ptr[k] != '\0' && ptr[k] >= 32 && ptr[k] <= 126 && k < 120) {
                                        zoneBuf[k] = ptr[k];
                                        k++;
                                    }
                                    zoneBuf[k] = '\0';
                                    std::string zoneStr(zoneBuf);

                                    if (zoneStr.length() > 17 && contains_ignore_case(zoneStr, filterStr)) {
                                        if (seenNames.find(zoneStr) == seenNames.end()) {
                                            seenNames.insert(zoneStr);
                                            matchCount++;
                                            unsigned long long addr = start + (ptr - buf);
                                            g_sessionZones.push_back({zoneStr, addr});

                                            std::cout << COLOR_GREEN << "  [" << std::setw(2) << matchCount << "] "
                                                      << COLOR_WHITE << std::left << std::setw(45) << zoneStr
                                                      << COLOR_YELLOW << " | RAM: " << fmt_hex(addr) << COLOR_RESET << std::endl;
                                        }
                                    }
                                    ptr += (k > 0 ? k : 17);
                                } else {
                                    ptr++;
                                }
                            }
                        }
                        free(buf);
                    }
                }
            }
        }
    }

    close(mem_fd);
    fclose(maps);

    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "[+] DUMP HOAN THANH! Tim thấy: " << g_sessionZones.size() << " khu vuc spawn độc nhất." << COLOR_RESET << std::endl;
}

// ================================================================
// MODULE 2: REALTIME ZONE WATCHER
// ================================================================

void realtime_zone_watcher(pid_t pid, int intervalSec) {
    std::cout << COLOR_YELLOW << "\n[⏱️] DANG CHAY REALTIME WATCHER (Moi " << intervalSec << "s)..." << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "Bấm Enter hoặc 'q' rồi Enter để quay lại menu." << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "--------------------------------------------------" << COLOR_RESET << std::endl;

    std::string lastZone = "";
    int loopCount = 0;

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    while (true) {
        loopCount++;
        FILE* maps = fopen(maps_path, "r");
        if (!maps) break;
        int mem_fd = open(mem_path, O_RDONLY);
        if (mem_fd < 0) { fclose(maps); break; }

        std::string currentZone = "";
        char line[512];

        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "rw-p")) {
                unsigned long long start, end;
                if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
                    size_t size = end - start;
                    if (size > 0 && size <= 10 * 1024 * 1024) {
                        char* buf = (char*)malloc(size);
                        if (buf) {
                            if (pread(mem_fd, buf, size, (off_t)start) > 0) {
                                char* ptr = buf;
                                char* end_ptr = buf + size - 17;
                                while (ptr < end_ptr) {
                                    if (memcmp(ptr, "DinoSpawnEntries_", 17) == 0) {
                                        char zbuf[128] = {0};
                                        int k = 0;
                                        while (ptr[k] >= 32 && ptr[k] <= 126 && k < 100) {
                                            zbuf[k] = ptr[k]; k++;
                                        }
                                        zbuf[k] = '\0';
                                        if (strlen(zbuf) > 17) {
                                            currentZone = std::string(zbuf);
                                            break;
                                        }
                                    }
                                    ptr++;
                                }
                            }
                            free(buf);
                        }
                    }
                }
            }
            if (!currentZone.empty()) break;
        }

        close(mem_fd);
        fclose(maps);

        if (!currentZone.empty() && currentZone != lastZone) {
            lastZone = currentZone;
            std::cout << COLOR_GREEN << "  [" << get_current_time() << "] [# " << loopCount << "] ZONE MOI: "
                      << COLOR_YELLOW << currentZone << COLOR_RESET << std::endl;
        } else if (currentZone.empty()) {
            std::cout << COLOR_RED << "  [" << get_current_time() << "] [# " << loopCount << "] Không tìm thấy Zone active" << COLOR_RESET << std::endl;
        }

        sleep(intervalSec);
    }
}

// ================================================================
// MODULE 3: AIM TARGET & CREATURE DUMPER
// ================================================================

void dump_creatures(pid_t pid, const std::string& filterStr) {
    std::cout << COLOR_CYAN << "\n==================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "[🎯] DUMP AIM TARGET & CREATURES IN RAM" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) return;
    int mem_fd = open(mem_path, O_RDONLY);
    if (mem_fd < 0) { fclose(maps); return; }

    const std::vector<std::string> patterns = {
        "APrimalDinoCharacter", "APrimalCharacter", "Dino_Character_BP_", "BP_Dino_"
    };

    g_sessionCreatures.clear();
    std::set<unsigned long long> seenAddrs;
    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "rw-p")) {
            unsigned long long start, end;
            if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
                size_t size = end - start;
                if (size > 0 && size <= 15 * 1024 * 1024) {
                    char* buf = (char*)malloc(size);
                    if (buf) {
                        if (pread(mem_fd, buf, size, (off_t)start) > 0) {
                            for (const auto& pat : patterns) {
                                size_t patLen = pat.length();
                                char* ptr = buf;
                                char* end_ptr = buf + size - patLen;

                                while (ptr < end_ptr) {
                                    if (memcmp(ptr, pat.c_str(), patLen) == 0) {
                                        char nameBuf[128] = {0};
                                        int k = 0;
                                        while (ptr[k] >= 32 && ptr[k] <= 126 && k < 120) {
                                            nameBuf[k] = ptr[k]; k++;
                                        }
                                        nameBuf[k] = '\0';
                                        std::string creatureName(nameBuf);

                                        unsigned long long addr = start + (ptr - buf);
                                        if (creatureName.length() > 5 &&
                                            contains_ignore_case(creatureName, filterStr) &&
                                            seenAddrs.find(addr) == seenAddrs.end()) {
                                            
                                            seenAddrs.insert(addr);
                                            count++;
                                            g_sessionCreatures.push_back({creatureName, addr});

                                            std::cout << COLOR_GREEN << "  [" << std::setw(2) << count << "] "
                                                      << COLOR_WHITE << std::left << std::setw(40) << creatureName
                                                      << COLOR_YELLOW << " | RAM: " << fmt_hex(addr) << COLOR_RESET << std::endl;
                                            if (count >= 30) break;
                                        }
                                    }
                                    ptr++;
                                }
                                if (count >= 30) break;
                            }
                        }
                        free(buf);
                    }
                }
            }
        }
        if (count >= 30) break;
    }

    close(mem_fd);
    fclose(maps);

    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "[+] Tìm thấy " << g_sessionCreatures.size() << " đối tượng creature." << COLOR_RESET << std::endl;
}

// ================================================================
// MODULE 4: PLAYER XYZ FINDER & TELEPORT
// ================================================================

void scan_player_z(pid_t pid, float targetZ, float delta) {
    std::cout << COLOR_CYAN << "\n==================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "[🚀] QUÉT ĐỊA CHỈ FLOAT Z NHÂN VẬT (" << targetZ << " +/- " << delta << ")" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) return;
    int mem_fd = open(mem_path, O_RDONLY);
    if (mem_fd < 0) { fclose(maps); return; }

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "rw-p")) {
            unsigned long long start, end;
            if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
                size_t size = end - start;
                if (size > 0 && size <= 10 * 1024 * 1024) {
                    char* buf = (char*)malloc(size);
                    if (buf) {
                        if (pread(mem_fd, buf, size, (off_t)start) > 0) {
                            float* fptr = (float*)buf;
                            size_t floatCount = size / sizeof(float);
                            for (size_t i = 0; i < floatCount; i++) {
                                float val = fptr[i];
                                if (val >= (targetZ - delta) && val <= (targetZ + delta)) {
                                    count++;
                                    unsigned long long addr = start + (i * sizeof(float));
                                    std::cout << COLOR_GREEN << "  [" << count << "] RAM: " << fmt_hex(addr)
                                              << "  | VALUE: " << std::fixed << std::setprecision(2) << val << COLOR_RESET << std::endl;
                                    
                                    if (count == 1) {
                                        g_sessionPlayer.addrZ = addr;
                                        g_sessionPlayer.valZ = val;
                                        g_sessionPlayer.addrX = addr - 8;
                                        g_sessionPlayer.addrY = addr - 4;
                                    }
                                    if (count >= 15) break;
                                }
                            }
                        }
                        free(buf);
                    }
                }
            }
        }
        if (count >= 15) break;
    }

    close(mem_fd);
    fclose(maps);

    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
    if (count > 0) {
        std::cout << COLOR_GREEN << "[+] Đã tự động lưu địa chỉ Z ứng viên: " << fmt_hex(g_sessionPlayer.addrZ) << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_RED << "[-] Không tìm thấy địa chỉ Float khớp với giá trị Z!" << COLOR_RESET << std::endl;
    }
}

void execute_teleport(pid_t pid, unsigned long long addrX, float valX,
                      unsigned long long addrY, float valY,
                      unsigned long long addrZ, float valZ) {
    bool okX = true, okY = true, okZ = true;
    if (addrX != 0) okX = write_float_to_ram(pid, addrX, valX);
    if (addrY != 0) okY = write_float_to_ram(pid, addrY, valY);
    if (addrZ != 0) okZ = write_float_to_ram(pid, addrZ, valZ);

    if (okX && okY && okZ) {
        std::cout << COLOR_GREEN << "\n[✔] TELEPORT THÀNH CÔNG!" << COLOR_RESET << std::endl;
        std::cout << "  X: " << fmt_hex(addrX) << " -> " << valX << std::endl;
        std::cout << "  Y: " << fmt_hex(addrY) << " -> " << valY << std::endl;
        std::cout << "  Z: " << fmt_hex(addrZ) << " -> " << valZ << std::endl;
    } else {
        std::cout << COLOR_RED << "\n[-] Teleport thất bại! Kiểm tra lại quyền Root hoặc địa chỉ RAM." << COLOR_RESET << std::endl;
    }
}

// ================================================================
// MODULE 5: VERIFY & PATCH MEMORY
// ================================================================

void verify_address(pid_t pid, unsigned long long address) {
    float fVal = 0.0f;
    unsigned int dVal = 0;

    bool okF = read_memory_bytes(pid, address, &fVal, sizeof(float));
    bool okD = read_memory_bytes(pid, address, &dVal, sizeof(unsigned int));

    std::cout << COLOR_CYAN << "\n--------------------------------------------------" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "[🔍] ĐỌC ĐỊA CHỈ: " << fmt_hex(address) << COLOR_RESET << std::endl;
    if (okF && okD) {
        std::cout << COLOR_GREEN << "  FLOAT : " << std::fixed << std::setprecision(6) << fVal << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  DWORD : " << dVal << " (Hex: 0x" << std::hex << dVal << std::dec << ")" << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_RED << "  [-] Không thể đọc vùng nhớ này (Invalid Address / Page Fault)" << COLOR_RESET << std::endl;
    }
    std::cout << COLOR_CYAN << "--------------------------------------------------" << COLOR_RESET << std::endl;
}

// ================================================================
// MODULE 6: SAVE SESSION
// ================================================================

void save_session() {
    const std::vector<std::string> targetPaths = {
        "/sdcard/ARK_Native_Dump.txt",
        "/data/local/tmp/ARK_Native_Dump.txt"
    };

    std::stringstream ss;
    ss << "==================================================\n";
    ss << "   ARK MOBILE NATIVE C++ DUMP SESSION REPORT\n";
    ss << "   Thời gian: " << get_current_time() << "\n";
    ss << "==================================================\n\n";

    ss << "--- 1. SPAWN ZONES DUMP (" << g_sessionZones.size() << ") ---\n";
    for (size_t i = 0; i < g_sessionZones.size(); i++) {
        ss << "[" << (i + 1) << "] " << g_sessionZones[i].name
           << " | RAM: " << fmt_hex(g_sessionZones[i].address) << "\n";
    }

    ss << "\n--- 2. CREATURE TARGETS (" << g_sessionCreatures.size() << ") ---\n";
    for (size_t i = 0; i < g_sessionCreatures.size(); i++) {
        ss << "[" << (i + 1) << "] " << g_sessionCreatures[i].type
           << " | RAM: " << fmt_hex(g_sessionCreatures[i].address) << "\n";
    }

    ss << "\n--- 3. PLAYER XYZ ADDRESSES ---\n";
    ss << "AddrX: " << fmt_hex(g_sessionPlayer.addrX) << " | ValX: " << g_sessionPlayer.valX << "\n";
    ss << "AddrY: " << fmt_hex(g_sessionPlayer.addrY) << " | ValY: " << g_sessionPlayer.valY << "\n";
    ss << "AddrZ: " << fmt_hex(g_sessionPlayer.addrZ) << " | ValZ: " << g_sessionPlayer.valZ << "\n";
    ss << "==================================================\n";

    std::string report = ss.str();
    bool saved = false;

    for (const auto& path : targetPaths) {
        FILE* f = fopen(path.c_str(), "w");
        if (f) {
            fputs(report.c_str(), f);
            fclose(f);
            std::cout << COLOR_GREEN << "[+] Đã lưu Session thành công vào: " << path << COLOR_RESET << std::endl;
            saved = true;
        }
    }

    if (!saved) {
        std::cout << COLOR_RED << "[-] Không thể ghi file session ra /sdcard/ hoặc /data/local/tmp/" << COLOR_RESET << std::endl;
    }
}

// ================================================================
// MAIN MENU & INTERACTIVE CLI
// ================================================================

void print_banner() {
    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_MAGENTA << "   ARK MOBILE MOD MENU — NATIVE C++ ENGINE v3.0" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "   GoGs Ultimate GSV  |  Android Root Terminal" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
}

int main() {
    print_banner();

    // Auto find PID
    std::cout << COLOR_YELLOW << "[*] Đang tự động dò tìm PID tiến trình ARK Mobile..." << COLOR_RESET << std::endl;
    g_currentPid = find_ark_pid();

    if (g_currentPid > 0) {
        std::cout << COLOR_GREEN << "[✔] Đã tìm thấy PID ARK: " << g_currentPid << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_RED << "[-] Không tìm thấy PID game ARK Mobile!" << COLOR_RESET << std::endl;
        std::cout << "Nhập thủ công PID game (hoặc gõ 0 để thoát): ";
        std::cin >> g_currentPid;
        if (g_currentPid <= 0) return 0;
    }

    while (true) {
        std::cout << COLOR_CYAN << "\n==================================================" << COLOR_RESET << std::endl;
        std::cout << COLOR_WHITE << " PID: " << COLOR_GREEN << g_currentPid
                  << COLOR_WHITE << " | Filter: " << COLOR_YELLOW << (g_targetFilter.empty() ? "(Tất cả)" : g_targetFilter)
                  << COLOR_WHITE << " | Zones: " << COLOR_MAGENTA << g_sessionZones.size() << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [1] Dump Danh Sách Khu Vực Spawn (DinoSpawnEntries_)" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [2] Bật Realtime Zone Watcher (Tự động theo dõi)" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [3] Dump Aim Target & Creatures Trong Vùng Nhớ" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [4] Dò Tìm Địa Chỉ Float Z & Teleport XYZ" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [5] Ghi Trực Tiếp Float Vào RAM (Patch Float)" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [6] Đọc & Kiểm Tra Địa Chỉ RAM (Verify Address)" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [7] Đặt Bộ Lọc Tên (Set Target Filter)" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  [8] Xuất File Session Báo Cáo (/sdcard/ARK_Native_Dump.txt)" << COLOR_RESET << std::endl;
        std::cout << COLOR_RED   << "  [0] Thoát Chương Trình" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "--------------------------------------------------" << COLOR_RESET << std::endl;
        std::cout << COLOR_WHITE << "Chọn chức năng [0-8]: " << COLOR_RESET;

        int choice = -1;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::string dummy;
            std::cin >> dummy;
            continue;
        }

        if (choice == 0) {
            std::cout << COLOR_YELLOW << "\nThoát công cụ Native Mod Menu. Tạm biệt!" << COLOR_RESET << std::endl;
            break;
        }

        switch (choice) {
            case 1: {
                dump_zones(g_currentPid, g_targetFilter);
                break;
            }
            case 2: {
                std::cout << "Nhập khoảng thời gian delay giữa các lần scan (giây, mặc định 3): ";
                int sec = 3;
                std::cin >> sec;
                if (sec < 1) sec = 1;
                realtime_zone_watcher(g_currentPid, sec);
                break;
            }
            case 3: {
                dump_creatures(g_currentPid, g_targetFilter);
                break;
            }
            case 4: {
                std::cout << "\nNhập giá trị Float Z hiển thị trên HUD ingame (ví dụ 200.0): ";
                float targetZ = 200.0f;
                std::cin >> targetZ;
                scan_player_z(g_currentPid, targetZ, 5.0f);

                if (g_sessionPlayer.addrZ != 0) {
                    std::cout << "\nBạn có muốn thực hiện Teleport ngay không? (1: Có, 0: Không): ";
                    int tpSel = 0;
                    std::cin >> tpSel;
                    if (tpSel == 1) {
                        std::cout << "Nhập giá trị Z mới (+2000 để bay lên): ";
                        float newZ = 2000.0f;
                        std::cin >> newZ;
                        execute_teleport(g_currentPid, g_sessionPlayer.addrX, 0,
                                         g_sessionPlayer.addrY, 0,
                                         g_sessionPlayer.addrZ, newZ);
                    }
                }
                break;
            }
            case 5: {
                std::cout << "\nNhập địa chỉ RAM (Hex, ví dụ 0x7f12a3b000): ";
                std::string hexStr;
                std::cin >> hexStr;
                unsigned long long addr = std::strtoull(hexStr.c_str(), NULL, 16);
                if (addr == 0) {
                    std::cout << COLOR_RED << "Địa chỉ Hex không hợp lệ!" << COLOR_RESET << std::endl;
                    break;
                }
                std::cout << "Nhập giá trị Float muốn ghi: ";
                float val = 0.0f;
                std::cin >> val;

                if (write_float_to_ram(g_currentPid, addr, val)) {
                    std::cout << COLOR_GREEN << "[✔] Đã ghi Float " << val << " vào địa chỉ " << fmt_hex(addr) << COLOR_RESET << std::endl;
                } else {
                    std::cout << COLOR_RED << "[-] Ghi thất bại! Lỗi truy cập bộ nhớ." << COLOR_RESET << std::endl;
                }
                break;
            }
            case 6: {
                std::cout << "\nNhập địa chỉ RAM cần kiểm tra (Hex, ví dụ 0x7f12a3b000): ";
                std::string hexStr;
                std::cin >> hexStr;
                unsigned long long addr = std::strtoull(hexStr.c_str(), NULL, 16);
                if (addr != 0) {
                    verify_address(g_currentPid, addr);
                } else {
                    std::cout << COLOR_RED << "Địa chỉ Hex không hợp lệ!" << COLOR_RESET << std::endl;
                }
                break;
            }
            case 7: {
                std::cout << "\nNhập tên Dino/Zone cần lọc (ví dụ: Beach, Rex, Redwood). Để trống = Không lọc: ";
                std::cin.ignore();
                std::getline(std::cin, g_targetFilter);
                std::cout << COLOR_GREEN << "[✔] Đã cập nhật bộ lọc thành: \"" << g_targetFilter << "\"" << COLOR_RESET << std::endl;
                break;
            }
            case 8: {
                save_session();
                break;
            }
            default: {
                std::cout << COLOR_RED << "Chức năng không hợp lệ!" << COLOR_RESET << std::endl;
                break;
            }
        }
    }

    return 0;
}
