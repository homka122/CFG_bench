#include "symbol_list.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Symbol symbol_create(char *str) {
    Symbol result;

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

void symbol_free(Symbol sym) { free(sym.label); }

SymbolList symbol_list_create() {
    SymbolList result;

    result.symbols = NULL;
    result.count = 0;

    return result;
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

int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i].label, str) == 0) {
            if (is_nonterm)
                list->symbols[i].is_nonterm = true;
            return i;
        }
    }

    Symbol new_sym = symbol_create(str);
    list->symbols = realloc(list->symbols, (list->count + 1) * sizeof(Symbol));
    if (is_nonterm)
        new_sym.is_nonterm = is_nonterm;
    list->symbols[list->count] = new_sym;

    return list->count++;
}

int symbol_list_add_str_start_nonterm(SymbolList *list, char *str) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i].label, str) == 0) {
            list->symbols[i].is_nonterm = true;
            Symbol temp = list->symbols[0];
            list->symbols[0] = list->symbols[i];
            list->symbols[i] = temp;

            return i;
        }
    }

    Symbol new_sym = symbol_create(str);
    list->symbols = realloc(list->symbols, (list->count + 1) * sizeof(Symbol));
    list->symbols[list->count] = list->symbols[0];
    list->symbols[0] = new_sym;
    new_sym.is_nonterm = true;
    list->count++;

    return (list->count - 1);
}

void symbol_list_swap(SymbolList *list, size_t i1, size_t i2) {
    Symbol temp = list->symbols[i1];
    list->symbols[i1] = list->symbols[i2];
    list->symbols[i2] = temp;
}

void symbol_list_free(SymbolList *list) {
    for (size_t i = 0; i < list->count; i++) {
        symbol_free(list->symbols[i]);
    }

    free(list->symbols);
}

bool is_non_terminal(const Symbol symbol, SymbolList non_terms) {
    for (size_t i = 0; i < non_terms.count; ++i) {
        if (strcmp(non_terms.symbols[i].label, symbol.label) == 0) {
            return true;
        }
    }

    return false;
}

bool is_number(const char *s) {
    if (s == NULL || *s == '\0')
        return false;
    while (*s) {
        if (!isdigit(*s))
            return false;
        s++;
    }
    return true;
}