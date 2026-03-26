#pragma once

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    char *label;
    bool is_indexed;
    bool is_nonterm;
} Symbol;

typedef struct {
    Symbol *symbols;
    size_t count;
} SymbolList;

Symbol symbol_create(char *str);
void symbol_free(Symbol sym);
SymbolList symbol_list_create();
void symbol_list_print(SymbolList list);
int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm);
int symbol_list_add_str_start_nonterm(SymbolList *list, char *str);
void symbol_list_swap(SymbolList *list, size_t i1, size_t i2);
void symbol_list_free(SymbolList *list);
bool is_non_terminal(const Symbol symbol, SymbolList non_terms);