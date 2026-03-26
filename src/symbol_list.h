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
    size_t capacity;
} SymbolList;

Symbol symbol_create(char *str);
void symbol_free(Symbol sym);
SymbolList symbol_list_create();
void symbol_list_print(SymbolList list);

// return index of Symbol with "str" label in SymbolList
// return -1 if Symbol with "str" label not found
int symbol_list_get_index_str(SymbolList *list, char *str);

// create Symbol with "str" label and put it in SymbolList
// if Symbol with "str" label already exists in SymbolList, return its index
//
// change nonterm field if fount term symbol with "str" label and "is_nonterm" is true
int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm);
void symbol_list_swap(SymbolList *list, size_t i1, size_t i2);
void symbol_list_free(SymbolList *list);
