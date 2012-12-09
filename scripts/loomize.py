#!/usr/bin/env python

import os, sys
from optparse import OptionParser

if __name__ == "__main__":

    # linkargs:
    # httpd: -lpthread -lexpat -lcrypt 
    # mysqld: -lpthread -lcrypt -ldl -lz
    parser = OptionParser(usage = "%prog <input: .bc> <output: executable> [options]")
    parser.add_option("", "--inline", action = "store_true", dest = "inline", default = False, help = "Inline the padding functions")
    parser.add_option("", "--cpp", action = "store_true", dest = "cpp", default = False, help = "Using CPP")
    parser.add_option("", "--linkargs", action = "store", type = "string", dest = "linkargs", default = "", help = "Arguments passed to the linker")
    (options, args) = parser.parse_args()

    if len(args) < 2:
        parser.print_usage()
        exit()

    input = args[0]
    output = args[1]
    func_cloned = input + "1"
    hook_injected = input + "2"
    padding_inlined = input + "3"

    print >> sys.stderr, "Stage 1: Cloning all functions in the program..."
    llvm_root_dir = os.getenv("LLVM_ROOT")
    os.system("opt -o " + func_cloned +
            " -load " + llvm_root_dir + "/install/lib/libid-manager.so " +
            " -load " + llvm_root_dir + "/install/lib/libloom-bit.so " +
            " -clone-func < " + input)
    
    print >> sys.stderr, "Stage 2: Injecting hook functions..."
    os.system("opt -o " + hook_injected +
            " -load " + llvm_root_dir + "/install/lib/libid-manager.so " +
            " -load " + llvm_root_dir + "/install/lib/libloom-bit.so " +
            " -inject-hook < " + func_cloned)

    print >> sys.stderr, "Stage 3: Generating the all-in-one .bc..."
    if options.inline:
        os.system("llvm-ld -o /tmp/loom_tmp " + hook_injected + " " + os.getenv("LOOM_ROOT") + "lib/loom-bit/stub.bc")
        os.system("mv /tmp/loom_tmp.bc " + padding_inlined)
    else:
        padding_inlined = hook_injected

    print >> sys.stderr, "Stage 4: Generating the assembly..."
    assembly = output + ".s"
    os.system("llc -f " + padding_inlined + " -o " + assembly)

    print >> sys.stderr, "Stage 5: Generating the executable..."
    if options.cpp:
        prog = "g++"
    else:
        prog = "gcc"
    os.system(prog + " -o " + output + " " + assembly + " " + os.environ["DEFENS_ROOT"] + "/loom-bit/loom.so " + options.linkargs)
