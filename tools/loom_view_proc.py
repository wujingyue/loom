#!/usr/bin/env python

import os

if __name__ == '__main__':
    print 'TID\tPID\tPPID\tCmd'
    os.system('ps c -eLo tid,pid,ppid,comm | grep loom')
