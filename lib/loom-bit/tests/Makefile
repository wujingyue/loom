OBJDIR=$(LLVM_ROOT)/llvm-obj/Release/lib

all: a.bc3 clone.bc3 race.bc3

%.bc: %.cpp
	llvm-g++ -g -O2 -emit-llvm -o $@ -c $<

%.bc: %.c
	llvm-gcc -g -O2 -emit-llvm -o $@ -c $<

%.bc1: %.bc $(OBJDIR)/clone-func.so $(OBJDIR)/idm.so
	opt -O2 -load $(OBJDIR)/idm.so -load $(OBJDIR)/clone-func.so -clone-func < $< > $@

%.bc2: %.bc1 $(OBJDIR)/inject-hook.so $(OBJDIR)/idm.so
	opt -O2 -load $(OBJDIR)/idm.so -load $(OBJDIR)/inject-hook.so -inject-hook < $< > $@

%.bc3: %.bc2 ../stub.bc
	llvm-ld -o tmp $^
	mv tmp.bc $@

%.s: %.bc3
	llc -f -o $@ $<

%.llvm: %.s
	g++ -o $@ $< ../loom.so -lpthread

clean:
	rm -f *.bc *.bc1 *.bc2 *.bc3 *.s *.llvm
