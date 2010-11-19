CFLAGS=-g -Wall
CXXFLAGS=-g -Wall

# BEGIN gdb
GDBSRC=${PWD}/gdb-7.0.1
GDBDST=${GDBSRC}/dist
GDBBUILDCOMPILERFLAGS = -g -fPIC
GDBBUILDOPTIONS = --prefix=${GDBDST} --without-expat   \
		   --disable-tui --disable-gdbtk --disable-sim \
		   --disable-multi-ice
ifeq ($(PLATFORM), x86)
CXXFLAGS += -m32 -DX86
CFLAGS += -m32 -DX86
GDBBUILDCOMPILERFLAGS += -m32
GDBBUILDOPTIONS += --build=i386-linux --target=i386-linux
else ifeq ($(PLATFORM), x86_64)
CXXFLAGS += -m64 -DX86_64
CFLAGS += -m64 -DX86_64
GDBBUILDCOMPILERFLAGS += -m64
GDBBUILDOPTIONS += --build=x86_64-linux --target=x86_64-linux
endif
INCLUDE_GDB=-I${GDBSRC}/include -I${GDBSRC}/gdb -I${GDBSRC}/gdb/config -I${GDBDST}/include  -I${GDBSRC}/bfd -I${GDBSRC}/gdb/common
LINK_GDB=$(GDBSRC)/gdb/libgdb.a \
	 ${GDBSRC}/bfd/libbfd.a \
	 ${GDBSRC}/opcodes/libopcodes.a \
	 ${GDBSRC}/libiberty/libiberty.a \
	 ${GDBSRC}/libdecnumber/libdecnumber.a \
	 ${GDBSRC}/readline/libreadline.a ${GDBSRC}/gdb/gnulib/libgnu.a
# END gdb

all: expr-parser
	make -C tests

find-symbol.o: find-symbol.c *.h

traverse.o: traverse.c *.h

line-to-addr.o: line-to-addr.c *.h

loom-exp.o: loom-exp.c *.h
	gcc $(CFLAGS) -o $@ -c $< $(INCLUDE_GDB)

parse-exp.o: parse-exp.c *.h
	gcc $(CFLAGS) -o $@ -c $< $(INCLUDE_GDB)

main.o: main.cpp *.h

expr-parser: main.o find-symbol.o traverse.o line-to-addr.o parse-exp.o loom-exp.o
	g++ $(CXXFLAGS) -o $@ $^ -ldwarf -lelf -ltermcap -lz -lpython2.6 $(LINK_GDB)

clean:
	rm -f *.o main
	make -C tests clean

buildgdb: 
	cd $(GDBSRC);make distclean;./configure ${GDBBUILDOPTIONS} CFLAGS="${GDBBUILDCOMPILERFLAGS}"
	make -C $(GDBSRC)
	make -C $(GDBSRC) install

