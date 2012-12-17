#!/usr/bin/env python

import socket, sys, struct

def print_usage():
    print 'Usage:'
    print '\tadd <fix ID> <extension name>'
    print '\tdel <fix ID>'
    print '\tquit or exit to exit the controller'

def send_message(conn, msg):
    buffer = struct.pack('!i', len(msg)) + msg
    n_sent = conn.send(buffer)
    if n_sent != len(buffer):
        return -1
    else:
        return 0

def recv_message(conn):
    str_pack_len = conn.recv(4)
    if len(str_pack_len) != 4:
        return -1, ''
    pack_len = struct.unpack('!i', str_pack_len)[0]
    if pack_len >= 1024:
        return -1, ''
    buffer = conn.recv(pack_len)
    if pack_len != len(buffer):
        return -1, ''
    return 0, buffer

if __name__ == '__main__':
    print_usage()
    HOST = 'localhost'
    PORT = 1229
    if len(sys.argv) >= 2:
        PORT = int(sys.argv[1])

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, PORT))
    sock.listen(1)
    print 'Waiting for connections...'
    conn, addr = sock.accept()
    print 'Connected by', addr
    while True:
        sys.stdout.write('\033[0;32mloom>\033[m ')
        cmd = sys.stdin.readline().strip()
        if cmd == 'quit' or cmd == 'exit':
            print 'exit'
            break
        if send_message(conn, cmd) == -1:
            print 'disconnected'
            break
        ret, buffer = recv_message(conn)
        if ret == -1:
            print 'disconnected'
            break
        print 'Response:', buffer
    sock.close()
