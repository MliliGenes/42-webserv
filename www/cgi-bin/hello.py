#!/usr/bin/env python3
import datetime
import os

print("Content-Type: text/html; charset=UTF-8")
print()
print("<!DOCTYPE html>")
print("<html lang='en'>")
print("<head><meta charset='UTF-8'><title>Python CGI</title></head>")
print("<body style='font-family: sans-serif; background:#0b1020; color:#eaf0ff; padding:40px;'>")
print("<h1>Python CGI works</h1>")
print(f"<p>Generated at {datetime.datetime.utcnow().isoformat()}Z</p>")
print(f"<p>Query string: {os.environ.get('QUERY_STRING', '')}</p>")
print("</body></html>")