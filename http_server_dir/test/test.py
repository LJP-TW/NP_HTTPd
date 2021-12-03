#!/usr/bin/env python3

import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("localhost", 5566))

http_req  = b'GET /sample_console.cgi HTTP/1.1\r\n'
http_req += b'Host: localhost:8787\r\n'
http_req += b'User-Agent: curl/7.74.0\r\n'
http_req += b'Accept: */*\r\n'
http_req += b'\r\n'

s.send(http_req)

while True:
    indata = s.recv(1024)
    if len(indata) == 0: # connection closed
        s.close()
        print('server closed connection.')
        break
    print(indata)

