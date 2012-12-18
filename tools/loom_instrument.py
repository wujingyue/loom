#!/usr/bin/env python

import os
import rcs_utils
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description = 'insert Loom update engine to the program')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    args = parser.parse_args()

    instrumented_bc = args.prog + '.loom.bc'
    instrumented_exe = args.prog + '.loom'

    # TODO: loom_utils.load_all_plugins
    cmd = rcs_utils.load_plugin('opt', 'RCSID')
    cmd = rcs_utils.load_plugin(cmd, 'RCSCFG')
    cmd = rcs_utils.load_plugin(cmd, 'LoomAnalysis')
    cmd = rcs_utils.load_plugin(cmd, 'LoomInstrumenter')
    cmd = ' '.join((cmd, '-break-crit-invokes', '-insert-checks', '-clone-bbs'))
    cmd = ' '.join((cmd, '-o', instrumented_bc))
    cmd = ' '.join((cmd, '<', args.prog + '.bc'))
    rcs_utils.invoke(cmd)

    cmd = ' '.join(('clang++', instrumented_bc,
                    rcs_utils.get_libdir() + '/libLoomUpdateEngine.a',
                    '-o', instrumented_exe))
    linking_flags = rcs_utils.get_linking_flags(args.prog)
    cmd = ' '.join((cmd, ' '.join(linking_flags), '-pthread'))
    rcs_utils.invoke(cmd) 
