#include "computed_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CACHE_DIR ".cache"
#define CACHE_FILE CACHE_DIR "/computed_results.csv"
#define KEY_SIZE 4096

// config paths are relative to the project root, so the cache is used only there
static bool is_project_root(void) { return access("src/test.c", F_OK) == 0; }

static long long file_mtime(const char *path) {
    struct stat st;
    if (path == NULL || stat(path, &st) != 0) {
        return 0;
    }
    return (long long)st.st_mtime;
}

static void make_key(char *key, const char *kind, const char *graph, const char *grammar, const char *start_nodes) {
    snprintf(key, KEY_SIZE, "%s,%s,%lld,%s,%lld,%s,%lld", kind, graph, file_mtime(graph), grammar, file_mtime(grammar),
             start_nodes == NULL ? "-" : start_nodes, file_mtime(start_nodes));
}

bool computed_cache_get(const char *kind, const char *graph, const char *grammar, const char *start_nodes,
                        size_t *value) {
    if (!is_project_root()) {
        return false;
    }

    FILE *file = fopen(CACHE_FILE, "r");
    if (file == NULL) {
        return false;
    }

    char key[KEY_SIZE];
    make_key(key, kind, graph, grammar, start_nodes);
    size_t key_len = strlen(key);

    bool found = false;
    char line[KEY_SIZE + 32];
    while (fgets(line, sizeof(line), file)) {
        // the last line with the key wins
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ',') {
            *value = strtoull(line + key_len + 1, NULL, 10);
            found = true;
        }
    }

    fclose(file);
    return found;
}

void computed_cache_put(const char *kind, const char *graph, const char *grammar, const char *start_nodes,
                        size_t value) {
    if (!is_project_root()) {
        return;
    }

    // an existing directory is fine, other errors are reported by fopen
    mkdir(CACHE_DIR, 0755);
    FILE *file = fopen(CACHE_FILE, "a");
    if (file == NULL) {
        fprintf(stderr, "Failed to open %s, the computed result is not cached\n", CACHE_FILE);
        return;
    }

    char key[KEY_SIZE];
    make_key(key, kind, graph, grammar, start_nodes);
    fprintf(file, "%s,%zu\n", key, value);
    fclose(file);
}
