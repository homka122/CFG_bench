#include <LAGraph.h>

typedef struct {
    size_t nonterms_count;
    size_t terms_count;
    size_t rules_count;
    LAGraph_rule_WCNF *rules;
} grammar_t;

void parser(char *config, grammar_t *grammar, GrB_Matrix **adj_matrices);