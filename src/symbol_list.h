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

// helper structure to accumulate data for building matrices
typedef struct {
    size_t *rows;
    size_t *cols;
    size_t *indeces;
    size_t size;
    size_t capacity;
} SymbolData;

Symbol symbol_create(char *str);
void symbol_free(Symbol *sym);

// return new string with numeration
// if symbol is not indexed, return its label
char *symbol_numerate(Symbol *sym, size_t num);

SymbolList symbol_list_create();
void symbol_list_print(SymbolList list);

// return index of Symbol with "str" label in SymbolList
// return -1 if Symbol with "str" label not found
int symbol_list_get_index_str(SymbolList *list, char *str);

// return label of Symbol with "index" in SymbolList
// raise error if Symbol with "index" not found
const char *symbol_list_get_str(const SymbolList *list, size_t index);

// create Symbol with "str" label and put it in SymbolList
// if Symbol with "str" label already exists in SymbolList, return its index
//
// change nonterm field if fount term symbol with "str" label and "is_nonterm" is true
int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm);
void symbol_list_swap(SymbolList *list, size_t i1, size_t i2);
void symbol_list_free(SymbolList *list);

// split SymbolList into two lists: one with terminal symbols and another with nonterminal symbols
// numerated from zero in order of occurrence in original list
void symbol_list_split(SymbolList *list, SymbolList *terms, SymbolList *nonterms);

// operations with SymbolData
SymbolData symbol_data_create(void);
void symbol_data_add(SymbolData *data, size_t row, size_t col, size_t index);
void symbol_data_free(SymbolData *data);
