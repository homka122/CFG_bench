#include "symbol_list.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Symbol symbol_create(char *str) {
    Symbol result = {0};

    size_t len = strlen(str);
    if (len >= 2) {
        result.is_indexed = strcmp(str + len - 2, "_i") == 0;
    } else {
        result.is_indexed = false;
    }

    result.label = malloc((len + 1) * sizeof(char));
    strcpy(result.label, str);
    result.is_nonterm = false;

    return result;
}

void symbol_free(Symbol sym) {
    free(sym.label);
    sym.label = NULL;
}

char *symbol_numerate(Symbol *sym, size_t num) {
    char *original_str = sym->label;
    size_t len = strlen(original_str);

    if (len >= 2 && strcmp(original_str + len - 2, "_i") == 0) {
        size_t num_len = snprintf(NULL, 0, "%zu", num);
        size_t new_len = len - 1 + num_len;

        char *new_str = malloc(new_len + 1);
        strncpy(new_str, original_str, len - 1);
        snprintf(new_str + len - 1, num_len + 1, "%zu", num);
        new_str[new_len] = '\0';

        return new_str;
    } else {
        return sym->label;
    }
}

SymbolList symbol_list_create() {
    SymbolList result;

    result.symbols = calloc(128, sizeof(Symbol));
    result.count = 0;
    result.capacity = 128;

    return result;
}

void symbol_list_free(SymbolList *list) {
    for (size_t i = 0; i < list->count; i++) {
        symbol_free(list->symbols[i]);
    }

    free(list->symbols);
    list->symbols = NULL;
    list->count = 0;
    list->capacity = 0;
}

void symbol_list_print(SymbolList list) {
    printf("Symbols list:\n");

    for (size_t i = 0; i < list.count; i++) {
        Symbol *sym = &list.symbols[i];
        char *indexed_str = sym->is_indexed ? " [I]" : "";
        char *nonterm_str = sym->is_nonterm ? " [N]" : " [T]";
        printf("%s%ld:\t%s\t%s\n", nonterm_str, i, sym->label, indexed_str);
    }
}

int symbol_list_get_index_str(SymbolList *list, char *str) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i].label, str) == 0) {
            return i;
        }
    }

    return -1;
}

int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm) {
    int index = symbol_list_get_index_str(list, str);
    if (index != -1) {
        if (is_nonterm)
            list->symbols[index].is_nonterm = true;
        return index;
    }

    Symbol new_sym = symbol_create(str);
    if (list->count == list->capacity) {
        list->capacity *= 2;
        list->symbols = realloc(list->symbols, list->capacity * sizeof(Symbol));
    }

    if (is_nonterm)
        new_sym.is_nonterm = is_nonterm;
    list->symbols[list->count++] = new_sym;

    return list->count - 1;
}

void symbol_list_swap(SymbolList *list, size_t i1, size_t i2) {
    Symbol temp = list->symbols[i1];
    list->symbols[i1] = list->symbols[i2];
    list->symbols[i2] = temp;
}
