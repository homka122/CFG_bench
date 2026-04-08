#include "grammar.h"
#include "symbol_list.h"
#include <stdio.h>
#include <string.h>

void grammar_print(Grammar grammar, SymbolList list) {
    printf("Grammar:\n");

    printf("Start nonterm: %s\n", list.symbols[grammar.start_nonterm].label);

    for (size_t i = 0; i < grammar.rules_count; i++) {
        int first = grammar.rules[i].first;
        int second = grammar.rules[i].second;
        int third = grammar.rules[i].third;

        if (third != -1) {
            printf("%s -> %s %s\n", list.symbols[first].label, list.symbols[second].label, list.symbols[third].label);
            continue;
        }

        if (second != -1) {
            printf("%s -> %s\n", list.symbols[first].label, list.symbols[second].label);
            continue;
        }

        printf("%s ->\n", list.symbols[first].label);
    }

    printf("\n");
}

void grammar_add_rule(Grammar *grammar, int first, int second, int third) {
    if (grammar->rules_count == grammar->rules_capacity) {
        grammar->rules_capacity = grammar->rules_capacity * 2 + 1;
        grammar->rules = realloc(grammar->rules, grammar->rules_capacity * sizeof(Rule));
    }

    grammar->rules[grammar->rules_count++] = (Rule){first, second, third};
}

// TODO: if there is no Count line
Grammar process_grammar(FILE *grammar_file, SymbolList *symbol_list) {
    Grammar result = {0};

    int start_nonterm = -1;
    char line[1024];

    while (fgets(line, sizeof(line), grammar_file)) {
        if (line[0] == '\n') {
            continue;
        }

        char *first = strtok(line, " \t\n");
        char *second = strtok(NULL, " \t\n");
        char *third = strtok(NULL, " \t\n");

        if (strcmp(first, "Count:") == 0) {
            fgets(line, sizeof(line), grammar_file);
            char *first = strtok(line, " \t\n");
            start_nonterm = symbol_list_add_str(symbol_list, first, true);

            break;
        }

        int first_symbol = symbol_list_add_str(symbol_list, first, true);
        int second_symbol = second ? symbol_list_add_str(symbol_list, second, false) : -1;
        int third_symbol = third ? symbol_list_add_str(symbol_list, third, false) : -1;
        
        grammar_add_rule(&result, first_symbol, second_symbol, third_symbol);
    }

    if (start_nonterm == -1) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m no start nonterminal in grammar file\n");
        exit(-1);
    }

    result.start_nonterm = start_nonterm;

    return result;
}

void grammar_swap_symbols(Grammar *grammar, int sym1, int sym2) {
    for (size_t i = 0; i < grammar->rules_count; i++) {
        if (grammar->rules[i].first == sym1) {
            grammar->rules[i].first = sym2;
        } else if (grammar->rules[i].first == sym2) {
            grammar->rules[i].first = sym1;
        }

        if (grammar->rules[i].second == sym1) {
            grammar->rules[i].second = sym2;
        } else if (grammar->rules[i].second == sym2) {
            grammar->rules[i].second = sym1;
        }

        if (grammar->rules[i].third == sym1) {
            grammar->rules[i].third = sym2;
        } else if (grammar->rules[i].third == sym2) {
            grammar->rules[i].third = sym1;
        }
    }
}