#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>

// Android Native C++ Memory Dumper for Unreal Engine (ARK Mobile)

pid_t get_process_pid(const char* process_name) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* ptr;
    while ((ptr = readdir(dir)) != NULL) {
        if (ptr->d_type == DT_DIR) {
            pid_t pid = atoi(ptr->d_name);
            if (pid > 0) {
                char cmdline_path[128];
                snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
                int fd = open(cmdline_path, O_RDONLY);
                if (fd >= 0) {
                    char cmdline[256] = {0};
                    read(fd, cmdline, sizeof(cmdline) - 1);
                    close(fd);
                    if (strstr(cmdline, process_name) != NULL) {
                        closedir(dir);
                        return pid;
                    }
                }
            }
        }
    }
    closedir(dir);
    return -1;
}

void dump_zones_native(pid_t pid) {
    char maps_path[128], mem_path[128];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    FILE* maps = fopen(maps_path, "r");
    if (!maps) {
        printf("[-] Khong the mo maps cua PID: %d\n", pid);
        return;
    }

    int mem_fd = open(mem_path, O_RDONLY);
    if (mem_fd < 0) {
        printf("[-] Khong the mo mem cua PID: %d (Can quyen Root!)\n", pid);
        fclose(maps);
        return;
    }

    printf("==================================================\n");
    printf("[+] DUMP NATIVE C++ KHU VUC REAL-TIME (PID: %d)\n", pid);
    printf("==================================================\n");

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "rw-p") || strstr(line, "r--p")) {
            unsigned long start, end;
            sscanf(line, "%lx-%lx", &start, &end);
            size_t size = end - start;

            if (size > 0 && size <= 20 * 1024 * 1024) { // Max 20MB region
                char* buffer = (char*)malloc(size);
                if (buffer) {
                    if (pread(mem_fd, buffer, size, start) > 0) {
                        char* ptr = buffer;
                        char* end_ptr = buffer + size - 17;

                        while (ptr < end_ptr) {
                            if (memcmp(ptr, "DinoSpawnEntries_", 17) == 0) {
                                char zone[128] = {0};
                                int k = 0;
                                while (ptr[k] != '\0' && ptr[k] >= 32 && ptr[k] <= 126 && k < 120) {
                                    zone[k] = ptr[k];
                                    k++;
                                }
                                zone[k] = '\0';

                                if (strlen(zone) > 17) {
                                    count++;
                                    printf("[%d] KHU VUC: %s\n", count, zone);
                                }
                                ptr += (k > 0 ? k : 17);
                            } else {
                                ptr++;
                            }
                        }
                    }
                    free(buffer);
                }
            }
        }
    }

    close(mem_fd);
    fclose(maps);

    printf("==================================================\n");
    printf("[+] NATIVE DUMP HOAN THANH! Tim thay %d khu vuc.\n", count);
    printf("==================================================\n");
}

int main() {
    pid_t pid = get_process_pid("studiowildcard");
    if (pid < 0) {
        pid = get_process_pid("ark");
    }

    if (pid < 0) {
        printf("[-] Khong tim thay tien trinh ARK Mobile trong Android!\n");
        return 1;
    }

    dump_zones_native(pid);
    return 0;
}
