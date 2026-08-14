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
// DEAR IMGUI NATIVE C++ MOD MENU ENGINE FOR ARK MOBILE v3.0
// Author: GoGs Ultimate GSV  |  Build: 2026-08-14
// ================================================================

struct ImGuiZone {
    std::string name;
    unsigned long long address;
};

// Helper: Find All ARK PIDs Case-Insensitively
static std::vector<pid_t> find_all_ark_pids() {
    std::vector<pid_t> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;

    struct dirent* ptr;
    while ((ptr = readdir(dir)) != NULL) {
        if (ptr->d_type == DT_DIR) {
            pid_t p = atoi(ptr->d_name);
            if (p > 0) {
                char cmdpath[128];
                snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", p);
                int fd = open(cmdpath, O_RDONLY);
                if (fd >= 0) {
                    char cmdline[512] = {0};
                    ssize_t bytes = read(fd, cmdline, sizeof(cmdline) - 1);
                    close(fd);
                    if (bytes > 0) {
                        for (ssize_t i = 0; i < bytes; i++) {
                            if (cmdline[i] == '\0') cmdline[i] = ' ';
                        }
                        std::string cmdStr(cmdline);
                        std::transform(cmdStr.begin(), cmdStr.end(), cmdStr.begin(), ::tolower);

                        if (cmdStr.find("studiowildcard") != std::string::npos ||
                            cmdStr.find("wardrum") != std::string::npos ||
                            cmdStr.find("ark") != std::string::npos ||
                            cmdStr.find("wildcard") != std::string::npos) {
                            pids.push_back(p);
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
    return pids;
}

// JNI Entry Point: Native Scan Zones
extern "C" JNIEXPORT jstring JNICALL
Java_com_gogs_ultimatedumper_FloatingModMenuService_nativeScanZones(JNIEnv* env, jobject thiz, jstring filter) {
    const char* filterCStr = env->GetStringUTFChars(filter, NULL);
    std::string filterStr = filterCStr ? std::string(filterCStr) : "";
    if (filterCStr) env->ReleaseStringUTFChars(filter, filterCStr);

    std::vector<pid_t> pids = find_all_ark_pids();
    if (pids.empty()) {
        return env->NewStringUTF("[-] Chưa tìm thấy tiến trình game ARK Mobile! (Hãy mở game ARK trước khi quét)");
    }

    std::set<std::string> seen;
    std::stringstream ss;
    ss << "=== DEAR IMGUI NATIVE ZONES SCAN (" << pids.size() << " PIDs) ===\n";

    int globalCount = 0;
    for (pid_t pid : pids) {
        char maps_path[128], mem_path[128];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
        snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

        FILE* maps = fopen(maps_path, "r");
        if (!maps) continue;
        int mem_fd = open(mem_path, O_RDONLY);
        if (mem_fd < 0) { fclose(maps); continue; }

        char line[512];
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
                                                globalCount++;
                                                unsigned long long addr = start + (ptr - buf);
                                                ss << "[" << std::setw(2) << globalCount << "] " << zName
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
    }

    if (globalCount == 0) {
        ss << "[-] Chưa tìm thấy Zone active. Hãy di chuyển nhân vật một chút!";
    } else {
        ss << "========================================\n[+] Tìm thấy " << globalCount << " khu vực spawn độc nhất!";
    }

    return env->NewStringUTF(ss.str().c_str());
}

// JNI Entry Point: Native Teleport Z
extern "C" JNIEXPORT jstring JNICALL
Java_com_gogs_ultimatedumper_FloatingModMenuService_nativeTeleportZ(JNIEnv* env, jobject thiz, jfloat targetZ, jfloat newZ) {
    std::vector<pid_t> pids = find_all_ark_pids();
    if (pids.empty()) return env->NewStringUTF("[-] Chưa tìm thấy PID game ARK Mobile!");

    int globalCount = 0;
    std::stringstream ss;
    ss << "=== DEAR IMGUI TELEPORT Z (Z: " << targetZ << " -> " << newZ << ") ===\n";

    for (pid_t pid : pids) {
        char maps_path[128], mem_path[128];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
        snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

        FILE* maps = fopen(maps_path, "r");
        if (!maps) continue;
        int mem_fd = open(mem_path, O_RDWR);
        if (mem_fd < 0) { fclose(maps); continue; }

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
                                float* fptr = (float*)buf;
                                size_t floatCount = size / sizeof(float);
                                for (size_t i = 0; i < floatCount; i++) {
                                    float val = fptr[i];
                                    if (val >= (targetZ - 5.0f) && val <= (targetZ + 5.0f)) {
                                        unsigned long long addr = start + (i * sizeof(float));
                                        float zValue = newZ;
                                        ssize_t ret = pwrite(mem_fd, &zValue, sizeof(float), (off_t)addr);
                                        if (ret == sizeof(float)) {
                                            globalCount++;
                                            ss << "  [✔] Patch RAM (PID " << pid << "): 0x" << std::hex << addr << std::dec << " | " << val << " -> " << newZ << "\n";
                                        }
                                        if (globalCount >= 5) break;
                                    }
                                }
                            }
                            free(buf);
                        }
                    }
                }
            }
            if (globalCount >= 5) break;
        }
        close(mem_fd);
        fclose(maps);
        if (globalCount >= 5) break;
    }

    if (globalCount > 0) {
        ss << "[✔] TELEPORT THÀNH CÔNG! Đã ghi " << globalCount << " địa chỉ RAM.";
    } else {
        ss << "[-] Không tìm thấy địa chỉ Z khớp!";
    }

    return env->NewStringUTF(ss.str().c_str());
}
