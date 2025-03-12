#include <GraphBLAS.h>
#include <LAGraph.h>
#include <LAGraphX.h>
#include <parser.h>
#include <time.h>

#define GRAPH_java_xalan "/java/xalan.g"
#define GRAPH_java_guava "/java/guava.g"
#define GRAPH_java_commons_lang3 "/java/commons_lang3.g"
#define GRAPH_java_commons_io "/java/commons_io.g"
#define GRAPH_java_pmd "/java/pmd.g"
#define GRAPH_java_tradebeans "/java/tradebeans.g"
#define GRAPH_java_tomcat "/java/tomcat.g"
#define GRAPH_java_mockito "/java/mockito.g"
#define GRAPH_java_h2 "/java/h2.g"
#define GRAPH_java_eclipse "/java/eclipse.g"
#define GRAPH_java_jackson "/java/jackson.g"
#define GRAPH_java_jython "/java/jython.g"
#define GRAPH_java_tradesoap "/java/tradesoap.g"
#define GRAPH_java_fop "/java/fop.g"
#define GRAPH_java_sunflow "/java/sunflow.g"
#define GRAPH_java_gson "/java/gson.g"
#define GRAPH_java_batik "/java/batik.g"
#define GRAPH_java_lusearch "/java/lusearch.g"
#define GRAPH_java_luindex "/java/luindex.g"
#define GRAPH_java_avrora "/java/avrora.g"
#define GRAPH_java_junit5 "/java/junit5.g"

#define run_algorithm()                                                        \
    LAGraph_CFL_reachability(outputs, adj_matrices, grammar.terms_count,       \
                             grammar.nonterms_count, grammar.rules,            \
                             grammar.rules_count, msg)

#define check_error(error)                                                     \
    {                                                                          \
        retval = run_algorithm();                                              \
        TEST_CHECK(retval == error);                                           \
        TEST_MSG("retval = %d (%s)", retval, msg);                             \
    }

#define check_result(result)                                                   \
    {                                                                          \
        char *expected = output_to_str(0);                                     \
        TEST_CHECK(strcmp(result, expected) == 0);                             \
        TEST_MSG("Wrong result. Actual: %s", expected);                        \
    }

#define NODES 2
#define EDGES 2

GrB_Matrix *adj_matrices = NULL;
GrB_Matrix *outputs = NULL;
grammar_t grammar = {0, 0, 0, NULL};
char msg[LAGRAPH_MSG_LEN];

void setup() { LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, msg); }

void teardown(void) { LAGraph_Finalize(msg); }

void init_outputs() {
    outputs = calloc(grammar.nonterms_count, sizeof(GrB_Matrix));
}

void free_outputs() {
    for (size_t i = 0; i < grammar.nonterms_count; i++) {
        if (outputs == NULL)
            break;

        if (outputs[i] == NULL)
            continue;

        GrB_free(&outputs[i]);
    }
    free(outputs);
    outputs = NULL;
}

void free_workspace() {

    for (size_t i = 0; i < grammar.terms_count; i++) {
        if (adj_matrices == NULL)
            break;

        if (adj_matrices[i] == NULL)
            continue;

        GrB_free(&adj_matrices[i]);
    }
    free(adj_matrices);
    adj_matrices = NULL;

    free_outputs();

    free(grammar.rules);
    grammar = (grammar_t){0, 0, 0, NULL};
}

void init_graph_3() {
    adj_matrices = calloc(2, sizeof(GrB_Matrix));
    GrB_Matrix adj_matrix_a, adj_matrix_b;
    GrB_Matrix_new(&adj_matrix_a, GrB_BOOL, 2, 2);
    GrB_Matrix_new(&adj_matrix_b, GrB_BOOL, 2, 2);

    GrB_Matrix_setElement(adj_matrix_a, true, 0, 1);
    GrB_Matrix_setElement(adj_matrix_a, true, 1, 0);
    GrB_Matrix_setElement(adj_matrix_b, true, 0, 0);

    adj_matrices[0] = adj_matrix_a;
    adj_matrices[1] = adj_matrix_b;
}

void init_huge_graph_go() {
    adj_matrices = calloc(4, sizeof(GrB_Matrix));
    GrB_Matrix adj_matrix_a, adj_matrix_b, adj_matrix_d, adj_matrix_c;
    GrB_Matrix_new(&adj_matrix_a, GrB_BOOL, 45007, 45007);
    GrB_Matrix_new(&adj_matrix_b, GrB_BOOL, 45007, 45007);
    GrB_Matrix_new(&adj_matrix_c, GrB_BOOL, 45007, 45007);
    GrB_Matrix_new(&adj_matrix_d, GrB_BOOL, 45007, 45007);

    FILE *fp;
    char line[255];

    fp = fopen("/home/homka122/code/spbu/course_work/test/go_hierarchy/"
               "go_hierarchy.csv",
               "r");
    if (fp == NULL)
        exit(-1);

    int line_i = 0;
    GrB_Index *row = malloc(sizeof(GrB_Index) * 10000000);
    GrB_Index *col = malloc(sizeof(GrB_Index) * 10000000);

    while (fgets(line, sizeof(line), fp) != NULL) {
        const int val1 = atoi(strtok(line, " "));
        const int val2 = atoi(strtok(NULL, " "));

        row[line_i] = val1;
        col[line_i] = val2;

        // GrB_Matrix_setElement(adj_matrix_a, true, val1, val2);
        // GrB_Matrix_setElement(adj_matrix_b, true, val2, val1);
        line_i++;
        if (line_i % 100000 == 0) {
            printf("%f%% %d\n", line_i / 490109.0 * 100, line_i);
        }
    }

    GrB_Scalar true_scalar;
    GrB_Scalar_new(&true_scalar, GrB_BOOL);
    GrB_Scalar_setElement_BOOL(true_scalar, true);
    GxB_Matrix_build_Scalar(adj_matrix_a, row, col, true_scalar, 490109);
    GxB_Matrix_build_Scalar(adj_matrix_b, col, row, true_scalar, 490109);

    GxB_print(adj_matrix_a, 1);
    GxB_print(adj_matrix_b, 1);
    free(row);
    free(col);
    fclose(fp);
    GrB_free(&true_scalar);
    adj_matrices[1] = adj_matrix_a;
    adj_matrices[0] = adj_matrix_b;
    adj_matrices[2] = adj_matrix_d;
    adj_matrices[3] = adj_matrix_c;
}

void init_huge_graph_taxonomy() {
    adj_matrices = calloc(4, sizeof(GrB_Matrix));
    GrB_Matrix adj_matrix_a, adj_matrix_b, adj_matrix_c, adj_matrix_d;
    GrB_Matrix_new(&adj_matrix_a, GrB_BOOL, 2112625, 2112625);
    GrB_Matrix_new(&adj_matrix_b, GrB_BOOL, 2112625, 2112625);
    GrB_Matrix_new(&adj_matrix_c, GrB_BOOL, 2112625, 2112625);
    GrB_Matrix_new(&adj_matrix_d, GrB_BOOL, 2112625, 2112625);

    FILE *fp;
    fp = fopen("/home/homka122/code/spbu/course_work/test/taxonomy_hierarchy/"
               "taxonomy_hierarchy.bin",
               "r");
    if (fp == NULL)
        exit(-1);

    int line_i = 0;
    GrB_Index *row = malloc(sizeof(GrB_Index) * 400000000);
    GrB_Index *col = malloc(sizeof(GrB_Index) * 400000000);

    int *buffer = malloc(sizeof(int) * 16 * 1024 * 1024);
    size_t count;
    while ((count = fread(buffer, sizeof(int), (16 * 1024 * 1024), fp)) != 0) {
        for (size_t i = 0; i < count; i += 2) {
            row[line_i] = buffer[i];
            col[line_i] = buffer[i + 1];
            line_i++;

            // if (line_i % 100000 == 0) {
            //     printf("%f%% %d\n", line_i / 32876289.0 * 100, line_i);
            // }
        }
    }

    GrB_Scalar true_scalar;
    GrB_Scalar_new(&true_scalar, GrB_BOOL);
    GrB_Scalar_setElement_BOOL(true_scalar, true);
    GxB_Matrix_build_Scalar(adj_matrix_a, row, col, true_scalar, 32876289);
    GxB_Matrix_build_Scalar(adj_matrix_b, col, row, true_scalar, 32876289);

    GxB_print(adj_matrix_a, 1);
    GxB_print(adj_matrix_b, 1);
    GxB_print(adj_matrix_d, 1);
    GxB_print(adj_matrix_c, 1);
    free(row);
    free(col);
    GrB_free(&true_scalar);
    fclose(fp);
    adj_matrices[1] = adj_matrix_a;
    adj_matrices[0] = adj_matrix_b;
    adj_matrices[2] = adj_matrix_d;
    adj_matrices[3] = adj_matrix_c;
}

void init_grammar_aSb() {
    LAGraph_rule_WCNF *rules = calloc(5, sizeof(LAGraph_rule_WCNF));
    rules[0] = (LAGraph_rule_WCNF){0, 1, 2, 0};
    rules[1] = (LAGraph_rule_WCNF){0, 1, 3, 0};
    rules[2] = (LAGraph_rule_WCNF){3, 0, 2, 0};
    rules[3] = (LAGraph_rule_WCNF){1, 0, -1, 0};
    rules[4] = (LAGraph_rule_WCNF){2, 1, -1, 0};

    grammar = (grammar_t){.nonterms_count = 4,
                          .terms_count = 2,
                          .rules_count = 5,
                          .rules = rules};
}

void init_grammar_vf() {
    LAGraph_rule_WCNF *rules = calloc(5, sizeof(LAGraph_rule_WCNF));
    rules[0] = (LAGraph_rule_WCNF){0, 1, 2, 0};
    rules[1] = (LAGraph_rule_WCNF){0, 1, 3, 0};
    rules[2] = (LAGraph_rule_WCNF){3, 0, 2, 0};
    rules[3] = (LAGraph_rule_WCNF){1, 0, -1, 0};
    rules[4] = (LAGraph_rule_WCNF){2, 1, -1, 0};

    grammar = (grammar_t){.nonterms_count = 4,
                          .terms_count = 2,
                          .rules_count = 5,
                          .rules = rules};
}

//====================
// Grammars
//====================

// S -> aSb | ab | cSd | cd in WCNF
//
// Terms: [0 a] [1 b] [2 c] [3 d]
// Nonterms: [0 S] [1 A] [2 B] [3 C] [4 D] [5 E] [6 F]
// S -> AB [0 1 2 0]
// S -> AC [0 1 3 0]
// C -> SB [3 0 2 0]
// A -> a  [1 0 -1 0]
// B -> b  [2 1 -1 0]
// S -> CD
// S -> CF
// F -> SD
// C -> c
// D -> d
void init_grammar_subclass() {
    // Allocate memory for the grammar rules
    LAGraph_rule_WCNF *rules = calloc(10, sizeof(LAGraph_rule_WCNF));

    // Define the grammar rules
    rules[0] = (LAGraph_rule_WCNF){0, 1, 2, 0};  // S -> AB
    rules[1] = (LAGraph_rule_WCNF){0, 1, 3, 0};  // S -> AC
    rules[2] = (LAGraph_rule_WCNF){3, 0, 2, 0};  // C -> SB
    rules[3] = (LAGraph_rule_WCNF){1, 0, -1, 0}; // A -> a
    rules[4] = (LAGraph_rule_WCNF){2, 1, -1, 0}; // B -> b
    rules[5] = (LAGraph_rule_WCNF){0, 4, 5, 0};  // S -> CD
    rules[6] = (LAGraph_rule_WCNF){0, 4, 6, 0};  // S -> CF
    rules[7] = (LAGraph_rule_WCNF){6, 0, 4, 0};  // F -> SD
    rules[8] = (LAGraph_rule_WCNF){3, 2, -1, 0}; // C -> c
    rules[9] = (LAGraph_rule_WCNF){4, 3, -1, 0}; // D -> d

    // Define the grammar structure
    grammar = (grammar_t){
        .nonterms_count = 7, // Number of non-terminals (S, A, B, C, D, F)
        .terms_count = 4,    // Number of terminals (a, b, c, d)
        .rules_count = 10,   // Total number of rules defined
        .rules = rules       // Pointer to the rules
    };
}

char *configs_rdf[] = {"data/graphs/rdf/go_hierarchy.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/taxonomy.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/eclass.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/go.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/taxonomy_hierarchy.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       NULL};

char *configs_java[] = {
    "data/graphs/java/eclipse.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/lusearch.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/luindex.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/commons_io.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/sunflow.g,data/grammars/java_points_to.cnf",
    NULL};

char *configs_c_alias[] = {
    "data/graphs/c_alias/init.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/block.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/fs.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/ipc.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/lib.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/mm.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/net.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/security.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/sound.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/arch.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/crypto.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/drivers.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/kernel.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/postgre.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/apache.g,data/grammars/c_alias.cnf",
    NULL};

char *configs_vf[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf",
                      "data/graphs/vf/nab.g,data/grammars/vf.cnf",
                      "data/graphs/vf/leela.g,data/grammars/vf.cnf", NULL};

char *configs_my[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf", NULL};

// Number of benchmark runs on a single graph
#define COUNT 10
// If true, the first run is done without measuring time (warm-up)
#define HOT true
// Use your custom configuration for the benchmark (default is the xz.g graph
// and vf.cnf grammar)
#define configs configs_my

int main(int argc, char **argv) {
    setup();
    GrB_Info retval;

    // char *config = argv[1];
    printf("Start bench\n");
    fflush(stdout);
    int config_index = 0;
    char *config = configs[config_index];
    while (config) {
        printf("CONFIG: %s\n", config);
        fflush(stdout);
        parser(config, &grammar, &adj_matrices);

        double start[COUNT];
        double end[COUNT];

        if (HOT) {
            run_algorithm();
        }

        GrB_Index nnz = 0;
        for (size_t i = 0; i < COUNT; i++) {
            init_outputs();

            start[i] = LAGraph_WallClockTime();
            retval = run_algorithm();
            end[i] = LAGraph_WallClockTime();

            GrB_Matrix_nvals(&nnz, outputs[0]);
            printf("\t%.3fs", end[i] - start[i]);
            fflush(stdout);
            free_outputs();
        }
        printf("\n");

        // printf("retval = %d (%s)\n", retval, msg);
        double sum = 0;
        for (size_t i = 0; i < COUNT; i++) {
            sum += end[i] - start[i];
        }

        printf("\tTime elapsed (avg): %.6f seconds. Result: %ld (return code "
               "%d) (%s)\n\n",
               sum / COUNT, nnz, retval, msg);
        // GxB_print(outputs[0], 1);
        free_workspace();
        config = configs[++config_index];
        fflush(stdout);
    }

    teardown();
}
