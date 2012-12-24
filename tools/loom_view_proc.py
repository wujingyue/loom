#!/usr/bin/env python

# FIXME: This script is not able to list a Loom process whose name is longer
# than 15 characters.

import os

if __name__ == '__main__':
    print 'TID\tPID\tPPID\tCmd'
    os.system('ps c -eLo tid,pid,ppid,comm | grep loom')
