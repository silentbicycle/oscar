PROJECT=	test_oscar
CFLAGS=		-Wall -pedantic -g -O2

# Build the static library with 'ar' or 'libtool'?
MAKE_LIB=	ar rcs
#MAKE_LIB=	libtool -static -o

# -----

all: ${PROJECT}

# Compile test.c (only) with -std=c99.
${PROJECT}: liboscar.a test.c
	${CC} -o ${PROJECT} test.c ${CFLAGS} -std=c99 ${LDFLAGS} liboscar.a

liboscar.a: oscar.o
	${MAKE_LIB} liboscar.a oscar.o

oscar.c: oscar.h

clean:
	rm -f *.o *.a ${PROJECT}
