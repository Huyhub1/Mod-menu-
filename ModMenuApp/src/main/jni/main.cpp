#include <jni.h>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <android/log.h>

#define LOG_TAG "ImGuiModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ================================================================
// DEAR IMGUI NATIVE C++ MOD MENU ENGINE FOR ARK MOBILE
// Author: GoGs Ultimate GSV  |  Build: 2026-08-14
// ================================================================

struct ImGuiZone {
    std::string name;
    unsigned long long address;
};

struct ImGuiCreature {
    std::string type;
    unsigned long long address;
};

static std::vector<ImGuiZone> g_zones;
static std::vector<ImGuiCreature> g_creatures;
static pid_t g_targetPid = -1;

// Helper: Find ARK PID
static pid_t find_ark_pid() {
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

// JNI Entry Point: Scan Zones
extern "C" JNIEXPORT jstring JNICALL
Java_com_gogs_ultimatedumper_FloatingModMenuService_nativeScanZones(JNIEnv* env, jobject thiz, jstring filter) {
    const char* filterCStr = env->GetStringUTFChars(filter, NULL);
    std::string filterStr = filterCStr ? std::string(filterCStr) : "";
    if (filterCStr) env->ReleaseStringUTFChars(filter, filterCStr);

    if (g_targetPid <= 0) g_targetPid = find_ark_pid();
    if (g_targetPid <= 0) {
        return env->NewStringUTF("[-] Không tìm thấy tiến trình game ARK Mobile đang chạy!");
    }

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", g_targetPid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", g_targetPid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) return env->NewStringUTF("[-] Lỗi mở /proc/[PID]/maps!");
    int mem_fd = open(mem_path, O_RDONLY);
    if (mem_fd < 0) { fclose(maps); return env->NewStringUTF("[-] Lỗi mở /proc/[PID]/mem (Cần Root)!"); }

    g_zones.clear();
    std::set<std::string> seen;
    char line[512];
    std::stringstream ss;
    ss << "=== DEAR IMGUI NATIVE ZONES SCAN (PID: " << g_targetPid << ") ===\n";

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
                                    std::string zName(zbuf);

                                    if (zName.length() > 17) {
                                        if (seen.find(zName) == seen.end()) {
                                            seen.insert(zName);
                                            count++;
                                            unsigned long long addr = start + (ptr - buf);
                                            g_zones.push_back({zName, addr});

                                            ss << "[" << std::setw(2) << count << "] " << zName
                                               << " | RAM: 0x" << std::hex << addr << std::dec << "\n";
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

    if (count == 0) {
        ss << "[-] Chưa quét được Zone nào. Hãy di chuyển nhân vật một chút!";
    } else {
        ss << "========================================\n[+] Tìm thấy " << count << " khu vực spawn độc nhất.";
    }

    return env->NewStringUTF(ss.str().c_str());
}

// JNI Entry Point: Teleport Z
extern "C" JNIEXPORT jstring JNICALL
Java_com_gogs_ultimatedumper_FloatingModMenuService_nativeTeleportZ(JNIEnv* env, jobject thiz, jfloat targetZ, jfloat newZ) {
    if (g_targetPid <= 0) g_targetPid = find_ark_pid();
    if (g_targetPid <= 0) return env->NewStringUTF("[-] Chưa tìm thấy PID game!");

    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", g_targetPid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", g_targetPid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) return env->NewStringUTF("[-] Lỗi mở maps!");
    int mem_fd = open(mem_path, O_RDWR);
    if (mem_fd < 0) { fclose(maps); return env->NewStringUTF("[-] Lỗi mở mem với quyền ghi O_RDWR!"); }

    char line[512];
    int count = 0;
    std::stringstream ss;
    ss << "=== DEAR IMGUI TELEPORT Z (Z: " << targetZ << " -> " << newZ << ") ===\n";

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
                                    float zValue = newZ;
                                    ssize_t ret = pwrite(mem_fd, &zValue, sizeof(float), (off_t)addr);
                                    if (ret == sizeof(float)) {
                                        count++;
                                        ss << "  [✔] Patch RAM: 0x" << std::hex << addr << std::dec << " | " << val << " -> " << newZ << "\n";
                                    }
                                    if (count >= 5) break;
                                }
                            }
                        }
                        free(buf);
                    }
                }
            }
        }
        if (count >= 5) break;
    }

    close(mem_fd);
    fclose(maps);

    if (count > 0) {
        ss << "[✔] TELEPORT THÀNH CÔNG! Đã ghi " << count << " địa chỉ RAM.";
    } else {
        ss << "[-] Không tìm thấy địa chỉ Z khớp!";
    }

    return env->NewStringUTF(ss.str().c_str());
}
