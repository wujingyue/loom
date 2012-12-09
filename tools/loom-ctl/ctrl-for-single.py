#!/usr/bin/env python

import socket, sys, struct

def print_usage():
    print 'Usage:'
    print '\tadd <fix ID> <extension name>'
    print '\tdel <fix ID>'
    print '\tPress ^C to exit'

def send_message(conn, msg):
    buffer = struct.pack('!i', len(msg) + 4) + msg
    n_sent = conn.send(buffer)
    if n_sent != len(buffer):
        return -1
    else:
        return 0

def recv_message(conn):
    str_pack_len = conn.recv(4)
    print "len(str_pack_len) =", len(str_pack_len)
    if len(str_pack_len) != 4:
        return -1, ''
    pack_len = struct.unpack('!i', str_pack_len)[0]
    if pack_len >= 1024:
        return -1, ''
    buffer = conn.recv(pack_len - 4)
    if pack_len != len(buffer) + 4:
        return -1, ''
    return 0, buffer

def Main():
    print_usage()
    HOST = 'localhost'
    if len(sys.argv) >= 2:
        PORT = int(sys.argv[1])
    else:
        PORT = 1221
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((HOST, PORT))
    sock.listen(1)
    print 'Waiting for connections...'
    conn, addr = sock.accept()
    print 'Connected by', addr
    rv = send_message(conn, 'get_name')
    assert rv == 0
    ret, buffer = recv_message(conn)
    assert ret == 0
    print 'Response:', buffer
    while True:
        cmd = sys.stdin.readline().strip()
        if send_message(conn, cmd) == -1:
            break
        ret, buffer = recv_message(conn)
        if ret == -1:
            break
        print 'Response:', buffer
    print addr, 'disconnected'

if __name__ == '__main__':
    Main()
