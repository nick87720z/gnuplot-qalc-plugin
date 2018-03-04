CC ?= gcc
CXX ?= g++
CXXFLAGS ?= -O3
LDFLAGS ?= -O3
GNUPLOT ?= gnuplot

pkgname=gnuplot-qalc-plugin

CXXFLAGS += -fpic -std=c++14 -fpermissive -I"${GNUPLOT_SOURCES_DIR}"/src `pkg-config --cflags libqalculate`
LIBS = -lstdc++ `pkg-config --libs libqalculate`
OBJS = ${pkgname}.o

all: ${pkgname}.so

.PHONY: clean test

${pkgname}.so: ${OBJS}
	${CC} ${LDFLAGS} -shared ${LIBS} ${pkgname}.o -o ${pkgname}.so

test: ${pkgname}.so
	${GNUPLOT} -p -c ./test_script

clean:
	rm -f ${OBJS} ${pkgname}.so

