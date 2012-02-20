PROJECT=	test_oscar
CFLAGS=		-Wall -pedantic -std=c99 -g -O2

all: ${PROJECT}

${PROJECT}: oscar.c test.o
	${CC} -o ${PROJECT} oscar.c test.o ${CFLAGS} ${LDFLAGS}

test.c: oscar.h

test.o: test.c

clean:
	rm -f *.o ${PROJECT}
