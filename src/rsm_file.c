#include "rsm_file.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDEX_SUFFIX "_i"

typedef struct {
    const char *name; // file name for messages
    size_t line_no;
    size_t block_count;
    CFG_RSM *rsm;
    char *box; // current box, NULL before the first [box] header
} Reader;

static void report(const Reader *reader, const char *message, const char *detail) {
    fprintf(stderr, "%s:%zu: %s", reader->name, reader->line_no, message);
    if (detail != NULL) {
        fprintf(stderr, " '%s'", detail);
    }
    fprintf(stderr, "\n");
}

static char *trim(char *line) {
    while (*line == ' ' || *line == '\t' || *line == '\r') {
        line++;
    }
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
    return line;
}

static bool is_skipped(const char *line) { return line[0] == '\0' || line[0] == '#'; }

// "[box NAME]" -> NAME (in place), NULL otherwise
static char *parse_box_header(char *line) {
    size_t len = strlen(line);
    if (strncmp(line, "[box", 4) != 0 || len < 6 || line[len - 1] != ']') {
        return NULL;
    }
    line[len - 1] = '\0';
    char *name = trim(line + 4);
    return name[0] == '\0' || strpbrk(name, " \t") != NULL ? NULL : name;
}

// "KEY: VALUE" -> VALUE (in place), NULL otherwise
static char *parse_keyword(char *line, const char *keyword) {
    size_t len = strlen(keyword);
    if (strncmp(line, keyword, len) != 0 || line[len] != ':') {
        return NULL;
    }
    char *value = trim(line + len + 1);
    return value[0] == '\0' ? NULL : value;
}

// "FROM --LABEL--> TO" -> the three tokens (in place), false otherwise
static bool parse_transition(char *line, char **from, char **label, char **to) {
    char *open = strstr(line, "--");
    char *close = open == NULL ? NULL : strstr(open + 2, "-->");
    if (close == NULL) {
        return false;
    }
    *open = '\0';
    *close = '\0';
    *from = trim(line);
    *label = trim(open + 2);
    *to = trim(close + 3);
    return (*from)[0] != '\0' && (*label)[0] != '\0' && (*to)[0] != '\0' && strpbrk(*from, " \t") == NULL &&
           strpbrk(*to, " \t") == NULL;
}

static bool is_indexed(const char *token) {
    size_t len = strlen(token);
    size_t suffix_len = strlen(INDEX_SUFFIX);
    return len > suffix_len && strcmp(token + len - suffix_len, INDEX_SUFFIX) == 0;
}

// "p_i" -> "p_<block>" for indexed tokens, a copy otherwise; the result is malloc'ed
static char *numerate(const char *token, size_t block) {
    if (!is_indexed(token)) {
        return strdup(token);
    }
    size_t base_len = strlen(token) - strlen(INDEX_SUFFIX);
    size_t size = base_len + 1 + snprintf(NULL, 0, "%zu", block) + 1;
    char *result = malloc(size);
    if (result == NULL) {
        fprintf(stderr, "out of memory\n");
        abort();
    }
    snprintf(result, size, "%.*s_%zu", (int)base_len, token, block);
    return result;
}

static CFG_RSM_Box *current_box(Reader *reader) {
    int box_i = symbol_list_get_index_str(&reader->rsm->nonterms, reader->box);
    return &reader->rsm->boxes.data[box_i];
}

static void ensure_state(Reader *reader, const char *state) {
    if (symbol_list_get_index_str(&current_box(reader)->states, state) == -1) {
        rsm_add_state(reader->rsm, reader->box, state);
    }
}

static void add_final_state(Reader *reader, const char *state) {
    ensure_state(reader, state);
    CFG_RSM_Box *box = current_box(reader);
    int state_i = symbol_list_get_index_str(&box->states, state);
    for (size_t i = 0; i < box->final_states.count; i++) {
        if (box->final_states.data[i] == (size_t)state_i) {
            return;
        }
    }
    rsm_add_final_state(reader->rsm, reader->box, state);
}

static bool add_transition(Reader *reader, const char *from, const char *label, const char *to) {
    CFG_RSM *rsm = reader->rsm;
    bool is_nonterm = symbol_list_get_index_str(&rsm->nonterms, label) != -1;
    if (!is_nonterm && symbol_list_get_index_str(&rsm->terms, label) == -1) {
        rsm_add_term(rsm, label);
    }
    ensure_state(reader, from);
    ensure_state(reader, to);

    CFG_RSM_Box *box = current_box(reader);
    size_t from_i = (size_t)symbol_list_get_index_str(&box->states, from);
    size_t label_i = (size_t)symbol_list_get_index_str(is_nonterm ? &rsm->nonterms : &rsm->terms, label);
    for (size_t i = 0; i < box->edges.count; i++) {
        CFG_Edge edge = box->edges.data[i];
        if (edge.start == from_i && edge.label == label_i && edge.is_term == !is_nonterm) {
            report(reader, "box is not deterministic, repeated transition from state", from);
            return false;
        }
    }
    rsm_add_edge(rsm, reader->box, from, to, label);
    return true;
}

// Expand the indexed tokens of a line and add the result to the current box.
// Every token is either a state or a label, both expanded the same way.
static bool add_line(Reader *reader, const char *from, const char *label, const char *to, bool is_final) {
    bool indexed = is_indexed(from) || (label != NULL && is_indexed(label)) || (to != NULL && is_indexed(to));
    size_t repeats = indexed ? reader->block_count : 1;
    for (size_t block = 0; block < repeats; block++) {
        char *from_k = numerate(from, block);
        char *label_k = label == NULL ? NULL : numerate(label, block);
        char *to_k = to == NULL ? NULL : numerate(to, block);
        bool ok = true;
        if (is_final) {
            add_final_state(reader, from_k);
        } else if (label == NULL) {
            ensure_state(reader, from_k);
        } else {
            ok = add_transition(reader, from_k, label_k, to_k);
        }
        free(from_k);
        free(label_k);
        free(to_k);
        if (!ok) {
            return false;
        }
    }
    return true;
}

// First pass: boxes become nonterminals in file order, so transitions can name a box defined later.
static bool collect_boxes(Reader *reader, const char *text) {
    char *copy = strdup(text);
    char *save = NULL;
    bool ok = true;
    reader->line_no = 0;
    for (char *raw = strtok_r(copy, "\n", &save); raw != NULL; raw = strtok_r(NULL, "\n", &save)) {
        reader->line_no++;
        char *name = parse_box_header(trim(raw));
        if (name == NULL) {
            continue;
        }
        if (symbol_list_get_index_str(&reader->rsm->nonterms, name) != -1) {
            report(reader, "duplicate box", name);
            ok = false;
            break;
        }
        if (symbol_list_get_index_str(&reader->rsm->terms, name) != -1) {
            report(reader, "box has the name of a graph label", name);
            ok = false;
            break;
        }
        rsm_add_nonterm(reader->rsm, name);
    }
    free(copy);
    return ok;
}

static bool read_lines(Reader *reader, const char *text) {
    char *copy = strdup(text);
    char *save = NULL;
    char *start_box = NULL;
    bool ok = true;
    reader->line_no = 0;
    for (char *raw = strtok_r(copy, "\n", &save); ok && raw != NULL; raw = strtok_r(NULL, "\n", &save)) {
        reader->line_no++;
        char *line = trim(raw);
        if (is_skipped(line)) {
            continue;
        }

        char *name = parse_box_header(line);
        if (name != NULL) {
            free(reader->box);
            reader->box = strdup(name);
            continue;
        }

        char *value = parse_keyword(line, "start");
        if (reader->box == NULL) {
            if (value == NULL) {
                report(reader, "expected 'start: <box>' or '[box <name>]', got", line);
                ok = false;
            } else {
                free(start_box);
                start_box = strdup(value);
            }
            continue;
        }

        if (value != NULL) {
            if (current_box(reader)->start_state != (size_t)-1) {
                report(reader, "box has two start states", reader->box);
                ok = false;
                continue;
            }
            // "start: p_i" would need one start state per block, which a box cannot have
            if (is_indexed(value)) {
                report(reader, "start state cannot be indexed", value);
                ok = false;
                continue;
            }
            ensure_state(reader, value);
            rsm_set_start_state(reader->rsm, reader->box, value);
            continue;
        }

        value = parse_keyword(line, "final");
        if (value != NULL) {
            char *final_save = NULL;
            for (char *state = strtok_r(value, ", \t", &final_save); state != NULL;
                 state = strtok_r(NULL, ", \t", &final_save)) {
                add_line(reader, state, NULL, NULL, true);
            }
            continue;
        }

        char *from, *label, *to;
        if (!parse_transition(line, &from, &label, &to)) {
            report(reader, "expected 'start:', 'final:' or '<from> --<label>--> <to>', got", line);
            ok = false;
            continue;
        }
        ok = add_line(reader, from, label, to, false);
    }

    if (ok && reader->rsm->nonterms.count == 0) {
        report(reader, "no [box <name>] sections found", NULL);
        ok = false;
    }
    if (ok) {
        const char *start = start_box == NULL ? "S" : start_box;
        if (symbol_list_get_index_str(&reader->rsm->nonterms, start) == -1) {
            report(reader, "start box is not defined", start);
            ok = false;
        } else {
            rsm_set_start_nonterm(reader->rsm, start);
        }
    }
    for (size_t i = 0; ok && i < reader->rsm->boxes.count; i++) {
        if (reader->rsm->boxes.data[i].start_state == (size_t)-1) {
            report(reader, "box has no start state", symbol_list_get_str(&reader->rsm->nonterms, i));
            ok = false;
        }
    }

    free(start_box);
    free(copy);
    return ok;
}

CFG_RSM *rsm_from_text(const char *text, const char *name, size_t block_count, SymbolList *terms) {
    Reader reader = {
        .name = name,
        .block_count = block_count == 0 ? 1 : block_count,
        .rsm = rsm_init(terms),
    };
    bool ok = collect_boxes(&reader, text) && read_lines(&reader, text);
    free(reader.box);
    if (!ok) {
        rsm_free(reader.rsm);
        return NULL;
    }
    return reader.rsm;
}

CFG_RSM *rsm_from_file(const char *path, size_t block_count, SymbolList *terms) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "cannot open RSM file %s\n", path);
        symbol_list_free(terms);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fprintf(stderr, "out of memory\n");
        abort();
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = '\0';

    CFG_RSM *rsm = rsm_from_text(text, path, block_count, terms);
    free(text);
    return rsm;
}
