#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

ssize_t mem_get_peak_kb(void) {
    FILE *file = fopen("/proc/self/status", "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening /proc/self/status\n");
        return -1;
    }

    ssize_t memory_usage_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "VmHWM: %zd kB", &memory_usage_kb) == 1) {
            break;
        }
    }

    fclose(file);
    return memory_usage_kb;
}

// reset peak memory usage by writing to /proc/self/clear_refs
int mem_peak_reset(void) {
    FILE *file = fopen("/proc/self/clear_refs", "w");
    if (file == NULL) {
        fprintf(stderr, "Error opening /proc/self/clear_refs\n");
        return -1;
    }

    // https://man7.org/linux/man-pages/man5/proc_pid_clear_refs.5.html
    int ret = fputs("5\n", file);

    fclose(file);
    return ret < 0 ? -1 : 0;
}
