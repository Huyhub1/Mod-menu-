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
// ARK MOBILE DEAR IMGUI NATIVE C++ ENGINE v3.0
// Author: GoGs Ultimate GSV  |  Build: 2026-08-14
// Platform: Android Root (Termux / ADB Shell / Native Executable)
// Features: ImGui GUI Style, RAM Scanner, Teleport, Online Cloud Config
// ================================================================

// ANSI Colors & Visual ImGui Style Tokens
#define IM_RESET   "\033[0m"
#define IM_CYAN    "\033[1;36m"
#define IM_GREEN   "\033[1;32m"
#define IM_YELLOW  "\033[1;33m"
#define IM_RED     "\033[1;31m"
#define IM_MAGENTA "\033[1;35m"
#define IM_WHITE   "\033[1;37m"
#define IM_BLUE    "\033[1;34m"

struct ImGuiZoneEntry {
    std::string name;
    unsigned long long address;
};

struct ImGuiCreatureEntry {
    std::string type;
    unsigned long long address;
};

struct ImGuiPlayerState {
    unsigned long long addrX = 0, addrY = 0, addrZ = 0;
    float valX = 0.0f, valY = 0.0f, valZ = 0.0f;
};

// Global ImGui Context State
static std::vector<ImGuiZoneEntry> g_imguiZones;
static std::vector<ImGuiCreatureEntry> g_imguiCreatures;
static ImGuiPlayerState g_imguiPlayer;
static std::string g_imguiFilter = "";
static pid_t g_imguiPid = -1;
static std::string g_onlineConfigUrl = "https://raw.githubusercontent.com/Huyhub1/Mod-menu-/main/menu_config.json";

// Helper: Format Hex Address String
static std::string fmt_hex(unsigned long long addr) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::hex << addr;
    return ss.str();
}

// Helper: Get Time Stamp String
static std::string get_timestamp() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", t);
    return std::string(buf);
}

// Helper: Case Insensitive Substring Check
static bool contains_str(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char c1, char c2) { return std::tolower(c1) == std::tolower(c2); }
    );
    return (it != haystack.end());
}

// ================================================================
// PID FINDER ENGINE
// ================================================================

pid_t find_ark_pid_imgui() {
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
// MEMORY READ / WRITE ENGINE
// ================================================================

bool write_ram_float(pid_t pid, unsigned long long address, float value) {
    char mem_path[128];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int fd = open(mem_path, O_RDWR);
    if (fd < 0) return false;

    ssize_t ret = pwrite(fd, &value, sizeof(float), (off_t)address);
    close(fd);
    return (ret == sizeof(float));
}

bool read_ram_bytes(pid_t pid, unsigned long long address, void* buffer, size_t size) {
    char mem_path[128];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
    int fd = open(mem_path, O_RDONLY);
    if (fd < 0) return false;

    ssize_t ret = pread(fd, buffer, size, (off_t)address);
    close(fd);
    return (ret == (ssize_t)size);
}

// ================================================================
// IMGUI MODULE 1: SPAWN ZONES SCANNER
// ================================================================

void imgui_dump_zones(pid_t pid, const std::string& filterStr) {
    std::cout << IM_CYAN << "\n┌──────────────────────────────────────────────────────────┐" << IM_RESET << std::endl;
    std::cout << IM_YELLOW << "│ [📍] IMGUI TAB 1: SPAWN ZONES SCANNER                    │" << IM_RESET << std::endl;
    std::cout << IM_CYAN << "├──────────────────────────────────────────────────────────┤" << IM_RESET << std::endl;
    std::cout << IM_WHITE << "│ PID: " << std::left << std::setw(8) << pid 
              << " │ Filter: " << std::left << std::setw(30) << (filterStr.empty() ? "(All)" : filterStr) << " │" << IM_RESET << std::endl;
    std::cout << IM_CYAN << "└──────────────────────────────────────────────────────────┘" << IM_RESET << std::endl;

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) {
        std::cout << IM_RED << "[-] Lỗi: Không thể mở /proc/" << pid << "/maps" << IM_RESET << std::endl;
        return;
    }
    int mem_fd = open(mem_path, O_RDONLY);
    if (mem_fd < 0) {
        std::cout << IM_RED << "[-] Lỗi: Không thể mở /proc/" << pid << "/mem (Cần quyền Root 'su')" << IM_RESET << std::endl;
        fclose(maps);
        return;
    }

    g_imguiZones.clear();
    std::set<std::string> seenNames;
    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "rw-p") || strstr(line, "r--p")) {
            unsigned long long start, end;
            if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
                size_t size = end - start;
                if (size > 0 && size <= 25 * 1024 * 1024) {
                    char* buf = (char*)malloc(size);
                    if (buf) {
                        if (pread(mem_fd, buf, size, (off_t)start) > 0) {
                            char* ptr = buf;
                            char* end_ptr = buf + size - 17;
                            while (ptr < end_ptr) {
                                if (memcmp(ptr, "DinoSpawnEntries_", 17) == 0) {
                                    char zbuf[128] = {0};
                                    int k = 0;
                                    while (ptr[k] >= 32 && ptr[k] <= 126 && k < 120) {
                                        zbuf[k] = ptr[k]; k++;
                                    }
                                    zbuf[k] = '\0';
                                    std::string zoneName(zbuf);

                                    if (zoneName.length() > 17 && contains_str(zoneName, filterStr)) {
                                        if (seenNames.find(zoneName) == seenNames.end()) {
                                            seenNames.insert(zoneName);
                                            count++;
                                            unsigned long long addr = start + (ptr - buf);
                                            g_imguiZones.push_back({zoneName, addr});

                                            std::cout << IM_GREEN << "  [" << std::setw(2) << count << "] "
                                                      << IM_WHITE << std::left << std::setw(42) << zoneName
                                                      << IM_YELLOW << " | RAM: " << fmt_hex(addr) << IM_RESET << std::endl;
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

    std::cout << IM_CYAN << "──────────────────────────────────────────────────────────" << IM_RESET << std::endl;
    std::cout << IM_GREEN << "[+] Quét thành công! Tìm thấy " << g_imguiZones.size() << " vùn spawn độc nhất." << IM_RESET << std::endl;
}

// ================================================================
// IMGUI MODULE 2: AIM TARGET & CREATURES
// ================================================================

void imgui_dump_creatures(pid_t pid, const std::string& filterStr) {
    std::cout << IM_CYAN << "\n┌──────────────────────────────────────────────────────────┐" << IM_RESET << std::endl;
    std::cout << IM_YELLOW << "│ [🎯] IMGUI TAB 2: AIM TARGET & CREATURE SCANNER          │" << IM_RESET << std::endl;
    std::cout << IM_CYAN << "└──────────────────────────────────────────────────────────┘" << IM_RESET << std::endl;

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

    g_imguiCreatures.clear();
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
                                        char cbuf[128] = {0};
                                        int k = 0;
                                        while (ptr[k] >= 32 && ptr[k] <= 126 && k < 120) {
                                            cbuf[k] = ptr[k]; k++;
                                        }
                                        cbuf[k] = '\0';
                                        std::string cname(cbuf);
                                        unsigned long long addr = start + (ptr - buf);

                                        if (cname.length() > 5 &&
                                            contains_str(cname, filterStr) &&
                                            seenAddrs.find(addr) == seenAddrs.end()) {

                                            seenAddrs.insert(addr);
                                            count++;
                                            g_imguiCreatures.push_back({cname, addr});

                                            std::cout << IM_GREEN << "  [" << std::setw(2) << count << "] "
                                                      << IM_WHITE << std::left << std::setw(40) << cname
                                                      << IM_YELLOW << " | RAM: " << fmt_hex(addr) << IM_RESET << std::endl;
                                            if (count >= 25) break;
                                        }
                                    }
                                    ptr++;
                                }
                                if (count >= 25) break;
                            }
                        }
                        free(buf);
                    }
                }
            }
        }
        if (count >= 25) break;
    }

    close(mem_fd);
    fclose(maps);

    std::cout << IM_CYAN << "──────────────────────────────────────────────────────────" << IM_RESET << std::endl;
    std::cout << IM_GREEN << "[+] Quét thành công! Tìm thấy " << g_imguiCreatures.size() << " đối tượng creature." << IM_RESET << std::endl;
}

// ================================================================
// IMGUI MODULE 3: TELEPORT XYZ & MEMORY PATCH
// ================================================================

void imgui_teleport_z(pid_t pid, float targetZ, float newZ) {
    std::cout << IM_CYAN << "\n┌──────────────────────────────────────────────────────────┐" << IM_RESET << std::endl;
    std::cout << IM_YELLOW << "│ [⚡] IMGUI TAB 3: TELEPORT XYZ (Target Z: " << targetZ << " -> " << newZ << ")   │" << IM_RESET << std::endl;
    std::cout << IM_CYAN << "└──────────────────────────────────────────────────────────┘" << IM_RESET << std::endl;

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) return;
    int mem_fd = open(mem_path, O_RDWR);
    if (mem_fd < 0) { fclose(maps); return; }

    char line[512];
    int matchCount = 0;

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
                                if (val >= (targetZ - 5.0f) && val <= (targetZ + 5.0f)) {
                                    unsigned long long addr = start + (i * sizeof(float));
                                    matchCount++;
                                    ssize_t ret = pwrite(mem_fd, &newZ, sizeof(float), (off_t)addr);
                                    if (ret == sizeof(float)) {
                                        std::cout << IM_GREEN << "  [✔] Patch RAM: " << fmt_hex(addr)
                                                  << " | " << val << " -> " << newZ << IM_RESET << std::endl;
                                    }
                                    if (matchCount >= 5) break;
                                }
                            }
                        }
                        free(buf);
                    }
                }
            }
        }
        if (matchCount >= 5) break;
    }

    close(mem_fd);
    fclose(maps);

    if (matchCount > 0) {
        std::cout << IM_GREEN << "[+] Teleport Z hoàn tất! Đã ghi thành công " << matchCount << " địa chỉ RAM." << IM_RESET << std::endl;
    } else {
        std::cout << IM_RED << "[-] Không tìm thấy địa chỉ Z khớp với giá trị " << targetZ << IM_RESET << std::endl;
    }
}

// ================================================================
// IMGUI MODULE 4: ONLINE CLOUD REMOTE CONFIG SYNC
// ================================================================

void imgui_sync_online_config() {
    std::cout << IM_CYAN << "\n┌──────────────────────────────────────────────────────────┐" << IM_RESET << std::endl;
    std::cout << IM_YELLOW << "│ [🌐] IMGUI TAB 4: ONLINE CLOUD REMOTE CONFIG SYNC        │" << IM_RESET << std::endl;
    std::cout << IM_CYAN << "└──────────────────────────────────────────────────────────┘" << IM_RESET << std::endl;
    std::cout << IM_WHITE << "URL: " << g_onlineConfigUrl << IM_RESET << std::endl;

    std::string cmd = "curl -s -m 5 \"" + g_onlineConfigUrl + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cout << IM_RED << "[-] Không thể thực thi lệnh curl trên Android!" << IM_RESET << std::endl;
        return;
    }

    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);

    if (!result.empty() && result.find("{") != std::string::npos) {
        std::cout << IM_GREEN << "[✔] ĐÃ ĐỒNG BỘ NỘI DUNG ONLINE TỪ GITHUB THÀNH CÔNG!" << IM_RESET << std::endl;
        std::cout << IM_YELLOW << "\n--- NỘI DUNG CONFIG ONLINE (" << get_timestamp() << ") ---" << IM_RESET << std::endl;
        if (result.length() > 600) {
            std::cout << result.substr(0, 600) << "\n... (truncated)" << std::endl;
        } else {
            std::cout << result << std::endl;
        }
    } else {
        std::cout << IM_RED << "[-] Không thể tải cấu hình từ URL GitHub! Kiểm tra lại mạng internet." << IM_RESET << std::endl;
    }
}

// ================================================================
// IMGUI MODULE 5: SESSION EXPORTER
// ================================================================

void imgui_export_session() {
    std::string filePath = "/sdcard/ARK_ImGui_Dump.txt";
    FILE* f = fopen(filePath.c_str(), "w");
    if (!f) {
        filePath = "/data/local/tmp/ARK_ImGui_Dump.txt";
        f = fopen(filePath.c_str(), "w");
    }

    if (!f) {
        std::cout << IM_RED << "[-] Không thể ghi file session báo cáo!" << IM_RESET << std::endl;
        return;
    }

    fprintf(f, "==================================================\n");
    fprintf(f, "  ARK MOBILE DEAR IMGUI NATIVE SESSION REPORT\n");
    fprintf(f, "  Thời gian: %s\n", get_timestamp().c_str());
    fprintf(f, "==================================================\n\n");

    fprintf(f, "--- 1. SPAWN ZONES (%zu) ---\n", g_imguiZones.size());
    for (size_t i = 0; i < g_imguiZones.size(); i++) {
        fprintf(f, "[%02zu] %s | RAM: %s\n", i + 1, g_imguiZones[i].name.c_str(), fmt_hex(g_imguiZones[i].address).c_str());
    }

    fprintf(f, "\n--- 2. CREATURE TARGETS (%zu) ---\n", g_imguiCreatures.size());
    for (size_t i = 0; i < g_imguiCreatures.size(); i++) {
        fprintf(f, "[%02zu] %s | RAM: %s\n", i + 1, g_imguiCreatures[i].type.c_str(), fmt_hex(g_imguiCreatures[i].address).c_str());
    }

    fprintf(f, "\n==================================================\n");
    fclose(f);

    std::cout << IM_GREEN << "\n[✔] ĐÃ XUẤT SESSION THÀNH CÔNG VÀO: " << filePath << IM_RESET << std::endl;
}

// ================================================================
// MAIN DEAR IMGUI NATIVE LOOP
// ================================================================

void render_imgui_menu_header() {
    std::cout << IM_CYAN << "┌──────────────────────────────────────────────────────────┐" << IM_RESET << std::endl;
    std::cout << IM_MAGENTA << "│      ARK ULTIMATE DEAR IMGUI NATIVE ENGINE v3.0          │" << IM_RESET << std::endl;
    std::cout << IM_WHITE << "│      Platform: Android Root  |  Author: GoGs Ultimate    │" << IM_RESET << std::endl;
    std::cout << IM_CYAN << "└──────────────────────────────────────────────────────────┘" << IM_RESET << std::endl;
}

int main() {
    render_imgui_menu_header();

    std::cout << IM_YELLOW << "[*] Đang tự động dò PID tiến trình ARK Mobile..." << IM_RESET << std::endl;
    g_imguiPid = find_ark_pid_imgui();

    if (g_imguiPid > 0) {
        std::cout << IM_GREEN << "[✔] Đã kết nối PID game: " << g_imguiPid << IM_RESET << std::endl;
    } else {
        std::cout << IM_RED << "[-] Không tự động tìm thấy PID game ARK Mobile!" << IM_RESET << std::endl;
        std::cout << "Nhập thủ công PID game (hoặc gõ 0 để thoát): ";
        std::cin >> g_imguiPid;
        if (g_imguiPid <= 0) return 0;
    }

    while (true) {
        std::cout << IM_CYAN << "\n==========================================================" << IM_RESET << std::endl;
        std::cout << IM_WHITE << " PID: " << IM_GREEN << g_imguiPid
                  << IM_WHITE << " │ Filter: " << IM_YELLOW << (g_imguiFilter.empty() ? "(All)" : g_imguiFilter)
                  << IM_WHITE << " │ Zones: " << IM_MAGENTA << g_imguiZones.size() << IM_RESET << std::endl;
        std::cout << IM_CYAN << "==========================================================" << IM_RESET << std::endl;
        std::cout << IM_GREEN << "  [1] 📍 ImGui Tab 1: Dump Danh Sách Spawn Zones (Realtime)" << IM_RESET << std::endl;
        std::cout << IM_GREEN << "  [2] 🎯 ImGui Tab 2: Dump Aim Target & Creatures" << IM_RESET << std::endl;
        std::cout << IM_GREEN << "  [3] ⚡ ImGui Tab 3: Dò Tìm Float Z & Teleport XYZ" << IM_RESET << std::endl;
        std::cout << IM_GREEN << "  [4] 🌐 ImGui Tab 4: Đồng Bộ Config Online GitHub" << IM_RESET << std::endl;
        std::cout << IM_GREEN << "  [5] 💾 ImGui Tab 5: Xuất Báo Cáo Session (/sdcard/)" << IM_RESET << std::endl;
        std::cout << IM_GREEN << "  [6] 🔍 Đặt Bộ Lọc Tên (Set Filter: Beach, Rex...)" << IM_RESET << std::endl;
        std::cout << IM_RED   << "  [0] 🚪 Thoát Chương Trình" << IM_RESET << std::endl;
        std::cout << IM_CYAN << "──────────────────────────────────────────────────────────" << IM_RESET << std::endl;
        std::cout << IM_WHITE << "Chọn tab chức năng ImGui [0-6]: " << IM_RESET;

        int choice = -1;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::string dummy;
            std::cin >> dummy;
            continue;
        }

        if (choice == 0) {
            std::cout << IM_YELLOW << "\nThoát ImGui Native Mod Menu. Tạm biệt!" << IM_RESET << std::endl;
            break;
        }

        switch (choice) {
            case 1: {
                imgui_dump_zones(g_imguiPid, g_imguiFilter);
                break;
            }
            case 2: {
                imgui_dump_creatures(g_imguiPid, g_imguiFilter);
                break;
            }
            case 3: {
                std::cout << "\nNhập giá trị Z hiện tại trên HUD (ví dụ 200.0): ";
                float curZ = 200.0f;
                std::cin >> curZ;
                std::cout << "Nhập giá trị Z mới (+2000 để bay lên cao): ";
                float newZ = 2000.0f;
                std::cin >> newZ;

                imgui_teleport_z(g_imguiPid, curZ, newZ);
                break;
            }
            case 4: {
                imgui_sync_online_config();
                break;
            }
            case 5: {
                imgui_export_session();
                break;
            }
            case 6: {
                std::cout << "\nNhập từ khóa lọc (ví dụ: Beach, Rex, Jungle). Để trống = Không lọc: ";
                std::cin.ignore();
                std::getline(std::cin, g_imguiFilter);
                std::cout << IM_GREEN << "[✔] Đã cập nhật bộ lọc ImGui: \"" << g_imguiFilter << "\"" << IM_RESET << std::endl;
                break;
            }
            default: {
                std::cout << IM_RED << "Lựa chọn không hợp lệ!" << IM_RESET << std::endl;
                break;
            }
        }
    }

    return 0;
}
