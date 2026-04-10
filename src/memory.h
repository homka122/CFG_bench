#pragma once

#include <stdio.h>
#include <stdlib.h>

// get peak memory usage in kilobytes by reading /proc/self/status
ssize_t mem_get_peak_kb();

// reset peak memory usage by writing to /proc/self/clear_refs
int mem_peak_reset();
