#!/usr/bin/env python3
import os
import sys

# ── read body ────────────────────────────────────────────────────────────────
body = ""
cl = os.environ.get("CONTENT_LENGTH", "0")
try:
    n = int(cl)
    if n > 0:
        body = sys.stdin.read(n)
except Exception:
    pass

# ── response headers ─────────────────────────────────────────────────────────
print("Content-Type: text/html; charset=utf-8")
print()

# ── response body ─────────────────────────────────────────────────────────────
print("""<style>
body { font-family: monospace; background: #0f0f0f; color: #e0e0e0; padding: 1.5rem; }
table { border-collapse: collapse; width: 100%; margin-bottom: 1.5rem; }
th { background: #1a1a2e; color: #e94560; text-align: left; padding: 0.5rem 0.8rem; }
td { padding: 0.4rem 0.8rem; border-bottom: 1px solid #2a2a4a; }
td:first-child { color: #60a5fa; width: 40%; }
h2 { color: #e94560; margin: 1rem 0 0.5rem; font-size: 1rem; }
pre { background: #1a1a2e; padding: 1rem; border-radius: 6px; white-space: pre-wrap;
      word-break: break-all; color: #4ade80; }
</style>""")

print("<h2>Request info</h2>")
print("<table>")

keys = [
    ("Method",         "REQUEST_METHOD"),
    ("Path",           "SCRIPT_NAME"),
    ("Query string",   "QUERY_STRING"),
    ("Content-Type",   "CONTENT_TYPE"),
    ("Content-Length", "CONTENT_LENGTH"),
    ("Server",         "SERVER_NAME"),
    ("Port",           "SERVER_PORT"),
    ("Protocol",       "SERVER_PROTOCOL"),
]
print("<tr><th>Field</th><th>Value</th></tr>")
for label, var in keys:
    val = os.environ.get(var, "—")
    print("<tr><td>{}</td><td>{}</td></tr>".format(label, val or "—"))
print("</table>")

print("<h2>HTTP headers (from env)</h2>")
print("<table><tr><th>Header</th><th>Value</th></tr>")
for k, v in sorted(os.environ.items()):
    if k.startswith("HTTP_"):
        name = k[5:].replace("_", "-").title()
        print("<tr><td>{}</td><td>{}</td></tr>".format(name, v))
print("</table>")

if body:
    print("<h2>Request body</h2>")
    print("<pre>{}</pre>".format(body.replace("<", "&lt;").replace(">", "&gt;")))
else:
    print("<p style='color:#666'>No body sent.</p>")
