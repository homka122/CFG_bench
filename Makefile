LIB_FLAGS = -lgraphblas -llagraph -llagraphx
INCLUDE_FLAGS = -I/usr/local/include/suitesparse -I./
ALGO_PATH=../LAGraph/experimental/algorithm/LAGraph_CFL_reachability_advanced.c
ALGO_OPT_PATH=../LAGraph/experimental/algorithm/LAGraph_CFL_optimized_matrix_opt.c
ALGO_I=-I../LAGraph/src/utility
ALGO_OBJ = LAGraph_CFL_reachability_advanced.o
ALGO_OPT_OBJ=LAGraph_CFL_optimized_matrix_opt.o
ALGO_OBJ_V = LAGraph_CFL_reachability_advanced_V.o

all: bench_compile_v

LAGraph_CFL_optimized_matrix_opt.o: ../LAGraph/experimental/algorithm/LAGraph_CFL_optimized_matrix_opt.c

$(ALGO_OBJ): ${ALGO_PATH} ${ALGO_OPT_OBJ}
	gcc ${ALGO_PATH} ${ALGO_OPT_PATH}  ${INCLUDE_FLAGS} ${ALGO_I} -o $@

$(ALGO_OPT_OBJ): ${ALGO_OPT_PATH}
	gcc -c ${ALGO_OPT_PATH} ${INCLUDE_FLAGS} ${ALGO_I} -o $@

$(ALGO_OBJ_V): ${ALGO_PATH}
	gcc -c -g  ${ALGO_PATH} -DBENCH_CFL_REACHBILITY ${INCLUDE_FLAGS} ${ALGO_I} -o $@

build: test.c parser.c
	gcc test.c parser.c ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} \
 -o test -O2 -Wextra -Wno-sign-compare -pedantic \

build-debug: test.c parser.c
	gcc test.c parser.c ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} \
 -o test -g -Wextra -Wno-sign-compare -pedantic -fsanitize=undefined \
 

run: test.c parser.c
	gcc test.c parser.c ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} \
 -o test -Wextra -Wno-sign-compare -pedantic -fsanitize=undefined -DDEBUG_parser \
 && ./test

bench: src/test.c src/parser.c
	gcc src/test.c src/parser.c -O2 ${LIB_FLAGS} ${INCLUDE_FLAGS} -o build/test

bench_compile: src/test.c src/parser.c ${ALGO_PATH} ${ALGO_OPT_PATH}
	gcc src/test.c src/parser.c ${ALGO_PATH} ${ALGO_OPT_PATH} ${ALGO_I} -O2 ${LIB_FLAGS} ${INCLUDE_FLAGS} -o build/test

bench_debug: src/test.c src/parser.c ${ALGO_PATH} ${ALGO_OPT_PATH}
	gcc src/test.c src/parser.c ${ALGO_PATH} ${ALGO_OPT_PATH} ${ALGO_I} -O0 -g ${LIB_FLAGS} ${INCLUDE_FLAGS} -o build/test

test_compile: test.c parser.c ${ALGO_OBJ}
	gcc test.c parser.c ${ALGO_OBJ} -O2 ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} ${ALGO_I} \
		-o test && ./test -${FLAGS}t

bench_compile_v: test.c parser.c ${ALGO_OBJ_V}
	gcc test.c parser.c ${ALGO_OBJ_V} -O2 -g ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} ${ALGO_I} \
		-o test && ./test -${FLAGS}

bench_compile_v_leak: test.c parser.c
	gcc test.c parser.c -fsanitize=address,leak ${ALGO_PATH} ${ALGO_OPT_PATH} -g -O2 ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} ${ALGO_I} \
		-o test && ./test -${FLAGS}

deb: test.c parser.c ${ALGO_PATH}
	gcc test.c parser.c -DBENCH_CFL_REACHBILITY -DDEBUG_parser -o test -O0 -g -Wextra -Wall -pedantic ${ALGO_PATH} ${LIB_FLAGS} ${INCLUDE_FLAGS} ${ALGO_I} && echo OK

bench-CI: test.c parser.c
	gcc test.c parser.c -O2 -DCI ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} \
		-o test && ./test

debug: test.c parser.c
	gcc test.c parser.c  ${ALGO} \
${LIB_FLAGS} ${INCLUDE_FLAGS} \
 -g -o test -Wextra -Wall -pedantic -fsanitize=address,undefined -DDEBUG \
 && ./test

debug-info: test.c parser.c
	gcc test.c parser.c  ${ALGO} \
${LIB_FLAGS} ${INCLUDE_FLAGS} \
 -g -o test -Wno-sign-compare -pedantic -DDEBUG \
 && ./test

convert: convert.c
	gcc convert.c -o convert -Wextra -Wall -pedantic && time ./convert
