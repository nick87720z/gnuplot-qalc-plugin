CC ?= gcc
CXX ?= g++
CXXFLAGS ?= -O3
LDFLAGS ?= ${CXXFLAGS}
GNUPLOT ?= gnuplot

LIBDIR ?= ${PREFIX}/${lib}

pkgname=gnuplot-qalc-plugin

CXXFLAGS += -Wall -fpic -std=c++14 -fpermissive -I"${GNUPLOT_SOURCES_DIR}"/src `pkg-config --cflags libqalculate`
LIBS = -lstdc++ `pkg-config --libs libqalculate`
OBJS = ${pkgname}.o

all: ${pkgname}.so

.PHONY: clean test install

${pkgname}.so: ${OBJS}
	${CC} ${LDFLAGS} -shared ${LIBS} ${pkgname}.o -o ${pkgname}.so

test: ${pkgname}.so
	${GNUPLOT} -p -c ./test_script

install: ${pkgname}.so
	install -D gnuplot-qalc-plugin.so ${DESTDIR}/${LIBDIR}

clean:
	rm -f ${OBJS} ${pkgname}.so

