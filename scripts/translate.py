#!/usr/bin/env python

import sys, re, string, os

if __name__ == "__main__":

    REGEX = "([^:]+):(\\d+)"
    lines = []
    for line in sys.stdin.readlines():
        lines.append(line)

    fin = open("/tmp/src-locator.in", "w")
    for line in lines:
        tokens = string.split(line)
        for token in tokens:
            mo = re.match(REGEX, token)
            if mo != None:
                fin.write("0 " + mo.group(1) + " " + mo.group(2) + "\n")
    fin.close()

    os.system("./opt.py idm src-locator src-locator-wrapper "
            "< mysqld-inject.bc > /dev/null")

    fout = open("/tmp/src-locator.out", "r")
    for line in lines:
        tokens = string.split(line)
        for token in tokens:
            mo = re.match(REGEX, token)
            if mo != None:
                ins_id = fout.readline().strip()
                sys.stdout.write(" " + mo.group(1) + ":" + ins_id)
            else:
                sys.stdout.write(" " + token)
        sys.stdout.write("\n")

