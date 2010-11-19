CFLAGS=-g -O2 -Wall -fPIC
CXXFLAGS=-g -O2 -Wall -fPIC

# We link <stub.o> to <loom.so> no matter whether we want to inline the padding functions.
# If the padding functions are inlined, the linker will automatically simply
# ignore the padding functions defined in <stub.o>
OBJ=stub.o loom.o fixes.o daemon.o sync_objs.o
STUB=stub.bc

LLVM_OBJ=$(LLVM_ROOT)/llvm-obj/
LLVM_EXTS=$(LLVM_OBJ)/Release/lib

TRACE_LIB=loom.so

all: $(TRACE_LIB) $(STUB) $(LLVM_EXTS)/idm.so
	make -C clone-func
	make -C inject-hook
	make -C mark-region
	make -C tests

$(TRACE_LIB): $(OBJ)
	g++ $(CXXFLAGS) -shared -Wl,-soname,$@ -o $@ $^

$(OBJ): *.h
%.o: %.cpp
	g++ $(CXXFLAGS) -o $@ -c $<

stub.bc: stub.cpp *.h
	llvm-g++ -O2 -emit-llvm -c $< -o $@

clean:
	make -C clone-func clean
	make -C inject-hook clean
	make -C mark-region clean
	make -C tests clean
	rm -f $(OBJ) $(TRACE_LIB) *~

