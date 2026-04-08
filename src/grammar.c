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
    fflush(stdout);
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

// convert rules M -> N t | M -> t N | M -> N to WCNF format
void grammar_to_WCNF(Grammar *grammar, SymbolList *list) {
// Code from CFPQ_PyAlgo by Ilya Muravyov
#define NONTERM_LABEL "NON_TERMINAL#"
    char *non_term_prefix = NONTERM_LABEL;
    char *eps_non_term_label = NONTERM_LABEL "EPS";
    bool is_eps_non_term_added = false;

    size_t terms_set_size = 0;
    size_t terms_set_capacity = 16;
    int *terms_set = malloc(sizeof(int) * terms_set_capacity);

    for (int i = 0; i < grammar->rules_count; ++i) {
        Rule *r = &grammar->rules[i];
        if (r->second == -1 && r->third == -1) {
            continue;
        } else if (r->second != -1 && r->third == -1) {
            if (!list->symbols[r->second].is_nonterm) {
                continue;
            }

            is_eps_non_term_added = true;
            int eps_index = symbol_list_add_str(list, eps_non_term_label, true);
            r->third = eps_index;
        } else if (r->second != -1 && r->third != -1) {
            if (list->symbols[r->second].is_nonterm && list->symbols[r->third].is_nonterm) {
                continue;
            }

            if (terms_set_size == terms_set_capacity) {
                terms_set_capacity *= 2;
                terms_set = realloc(terms_set, sizeof(int) * terms_set_capacity);
            }

            if (!list->symbols[r->second].is_nonterm && !list->symbols[r->third].is_nonterm) {
                char *term_nonterm_label1 =
                    malloc(strlen(non_term_prefix) + strlen(list->symbols[r->second].label) + 1);
                sprintf(term_nonterm_label1, "%s%s", non_term_prefix, list->symbols[r->second].label);
                int term_nonterm_index1 = symbol_list_add_str(list, term_nonterm_label1, true);
                free(term_nonterm_label1);

                char *term_nonterm_label2 = malloc(strlen(non_term_prefix) + strlen(list->symbols[r->third].label) + 1);
                sprintf(term_nonterm_label2, "%s%s", non_term_prefix, list->symbols[r->third].label);
                int term_nonterm_index2 = symbol_list_add_str(list, term_nonterm_label2, true);
                free(term_nonterm_label2);

                terms_set[terms_set_size++] = r->second;
                terms_set[terms_set_size++] = r->third;
                r->second = term_nonterm_index1;
                r->third = term_nonterm_index2;
                continue;
            }

            int *term_index;
            if (!list->symbols[r->second].is_nonterm) {
                term_index = &r->second;
            } else {
                term_index = &r->third;
            }

            char *term_nonterm_label = malloc(strlen(non_term_prefix) + strlen(list->symbols[*term_index].label) + 1);
            sprintf(term_nonterm_label, "%s%s", non_term_prefix, list->symbols[*term_index].label);
            int term_nonterm_index = symbol_list_add_str(list, term_nonterm_label, true);
            free(term_nonterm_label);

            terms_set[terms_set_size++] = *term_index;
            *term_index = term_nonterm_index;
        }
    }

    if (is_eps_non_term_added) {
        int eps_index = symbol_list_add_str(list, eps_non_term_label, true);
        grammar_add_rule(grammar, eps_index, -1, -1);
    }

    for (size_t i = 0; i < terms_set_size; i++) {
        int term_index = terms_set[i];
        char *term_nonterm_label = malloc(strlen(non_term_prefix) + strlen(list->symbols[term_index].label) + 1);
        sprintf(term_nonterm_label, "%s%s", non_term_prefix, list->symbols[term_index].label);
        int term_nonterm_index = symbol_list_add_str(list, term_nonterm_label, true);
        free(term_nonterm_label);

        grammar_add_rule(grammar, term_nonterm_index, term_index, -1);
    }

    free(terms_set);
}
