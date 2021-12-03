#!/usr/bin/env python3

import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("localhost", 5566))

uri = b'/console.cgi?h0=127.0.0.1&p0=1234&f0=t1.txt&h1=127.0.0.1&p1=2345&f1=t2.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4='
http_req  = b'GET '
http_req += uri
http_req += b' HTTP/1.1\r\n'
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

