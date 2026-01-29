#include <LAGraph.h>
#include <LAGraphX.h>

#define LG_ERROR_MSG(...)                                                      \
    {                                                                          \
        if (msg != NULL && msg[0] == '\0') {                                   \
            snprintf(msg, LAGRAPH_MSG_LEN, __VA_ARGS__);                       \
        }                                                                      \
    }

#ifndef GRB_CATCH
#define GRB_CATCH(info)                                                        \
    {                                                                          \
        LG_ERROR_MSG("GraphBLAS failure (file %s, line %d): info: %d",         \
                     __FILE__, __LINE__, info);                                \
        fprintf(stderr, "%s\n", msg);                                          \
        fflush(stderr);                                                        \
        exit(info);                                                            \
    }
#endif

#define MY_GRB_TRY(GrB_method)                                                 \
    {                                                                          \
        GrB_Info LG_GrB_Info = GrB_method;                                     \
        if (LG_GrB_Info < GrB_SUCCESS) {                                       \
            GRB_CATCH(LG_GrB_Info);                                            \
        }                                                                      \
    }

typedef struct {
    size_t nonterms_count;
    size_t rules_count;
    LAGraph_rule_EWCNF *rules;
} grammar_t;

typedef struct {
    char *label;
    bool is_indexed;
    bool is_nonterm;
} Symbol;

typedef struct {
    Symbol *symbols;
    size_t count;
} SymbolList;

typedef struct {
    int first;
    int second;
    int third;
} Rule;

typedef struct {
    int start_nonterm;
    Rule *rules;
    size_t rules_count;
} Grammar;

typedef struct {
    size_t u;
    size_t v;
    size_t term_index;
    size_t index;
} GraphEdge;

typedef struct {
    GraphEdge *edges;
    size_t edge_count;
    size_t node_count;
    size_t block_count;
} Graph;

typedef struct {
    size_t node_count;
    size_t block_count;
    Grammar grammar;
    SymbolList symbols;
    Graph graph;
} ParserResult;

ParserResult parser(char *config_i);

// SymbolList ans Symbol methods
Symbol symbol_create(char *str);
void symbol_free(Symbol sym);
SymbolList symbol_list_create();
void symbol_list_print(SymbolList list);
int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm);
int symbol_list_add_str_start_nonterm(SymbolList *list, char *str);
void symbol_list_free(SymbolList *list);
bool is_non_terminal(const Symbol symbol, SymbolList non_terms);
void grammar_print(Grammar grammar, SymbolList list);