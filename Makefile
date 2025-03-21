COUNT=10

LIB_FLAGS = -lgraphblas -llagraph -llagraphx
INCLUDE_FLAGS = -I/usr/local/include/suitesparse -I./

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

bench: test.c parser.c
	gcc test.c parser.c -O2 ${ALGO} ${LIB_FLAGS} ${INCLUDE_FLAGS} \
		-o test && ./test

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
