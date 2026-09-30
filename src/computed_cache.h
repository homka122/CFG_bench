#pragma once

#include <stdbool.h>
#include <stddef.h>

// cache for results computed with --compute-results
//
// values are stored in .cache/computed_results.csv, one "<key>,<value>" line each;
// config paths are relative, so the cache is used only when the benchmark runs from the project root
//
// the key contains "kind" (what the value counts), the graph, grammar and start nodes paths
// and their modification times, so a value is recomputed after any of these files changes;
// "start_nodes" is NULL when the value does not depend on start vertices

// returns true and sets "value" if the cache has a value for the key
bool computed_cache_get(const char *kind, const char *graph, const char *grammar, const char *start_nodes,
                        size_t *value);

// saves "value" for the key
void computed_cache_put(const char *kind, const char *graph, const char *grammar, const char *start_nodes,
                        size_t value);
