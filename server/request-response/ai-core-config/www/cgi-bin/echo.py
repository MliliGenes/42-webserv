#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/html")
print()

print("<h1>CGI Echo</h1>")

print("<h2>Method:</h2>")
print(os.environ.get("REQUEST_METHOD"))

print("<h2>Query:</h2>")
print(os.environ.get("QUERY_STRING"))

print("<h2>Body:</h2>")
print(sys.stdin.read())