Building Loom
====================

Build LLVM 3.0/3.1 and clang 3.0/3.1.

Build repo rcs. Checkout `git@github.com:wujingyue/rcs.git` and follow the
readme there.

Finally, build Loom:

    ./configure --with-rcssrc=<rcs srouce directory> --with-rcsobj=<rcs object directory> --prefix=`llvm-config --prefix`
    make
    make install

Running Loom
==================

Injects Loom's update engine into the application. For example,

    loom_instrument.py httpd

Start Loom's controller server:

    loom_ctl

Start the instrumented application. For example,
    ./httpd.loom

After the instrumented application starts, update it with execution filters.
For example,

    loom_ctl -add <some pid> <some filter file>
    loom_ctl -delete <some pid> <filter ID>
    loom_ctl -help to see more

Utilities
=========

loom\_view\_proc.py lists all Loom threads, including all threads in the
instrumented application and all Loom's daemon threads.

loom\_simple\_ctl.py is a simple controller that only supports singlethreaded
programs. See the startup message for usage.

Format of Loom Execution Filter
===============================
See `eval/template.lm`.
