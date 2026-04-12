#pragma once

#include "symbol_list.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int first;
    int second;
    int third;
} Rule;

typedef struct {
    int start_nonterm;
    Rule *rules;
    size_t rules_count;
    size_t rules_capacity;
} Grammar;

void grammar_print(Grammar grammar, SymbolList list);
void grammar_print_splited(Grammar grammar, SymbolList nonterms, SymbolList terms);
Grammar process_grammar(FILE *grammar_file, SymbolList *symbol_list);
void grammar_swap_symbols(Grammar *grammar, int sym1, int sym2);
void grammar_add_rule(Grammar *grammar, int first, int second, int third);
void grammar_to_WCNF(Grammar *grammar, SymbolList *list);
void grammar_split_terms_nonterms(Grammar *grammar, SymbolList *list, SymbolList *terms, SymbolList *nonterms);
