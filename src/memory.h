#pragma once

#include <stdio.h>

// get peak memory usage in kilobytes by reading /proc/self/status
ssize_t mem_get_peak_kb(void);

// reset peak memory usage by writing to /proc/self/clear_refs
int mem_peak_reset(void);
