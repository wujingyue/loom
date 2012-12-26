#!/usr/bin/env python

import os
import sys
import rcs_utils
import argparse

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'compile .lm to .filter')
    parser.add_argument('bc', help = 'the bitcode file')
    parser.add_argument('lm', help = '.lm file')
    args = parser.parse_args()
    
    if not args.lm.endswith('.lm'):
        print >> sys.stderr, 'The input file should end with .lm'
        sys.exit(1)

    # TODO: loom_utils.load_all_plugins
    cmd = rcs_utils.load_plugin('opt', 'RCSID')
    cmd = rcs_utils.load_plugin(cmd, 'LoomCompiler')
    cmd = ' '.join((cmd, '-compile'))
    cmd = ' '.join((cmd, '-lm', args.lm))
    cmd = ' '.join((cmd, '-analyze', '-q'))
    cmd = ' '.join((cmd, '<', args.bc))
    cmd = ' '.join((cmd, '>', os.path.splitext(args.lm)[0] + '.filter'))
    rcs_utils.invoke(cmd)
