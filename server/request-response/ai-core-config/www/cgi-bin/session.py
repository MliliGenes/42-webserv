#!/usr/bin/env python3
import os

# ── read cookie header ────────────────────────────────────────────────────────
cookie_header = os.environ.get("HTTP_COOKIE", "")
session_id = ""
for part in cookie_header.split(";"):
    part = part.strip()
    if part.startswith("session_id="):
        session_id = part[len("session_id="):]
        break

print("Content-Type: text/html; charset=utf-8")
print()

print("""<style>
body { font-family: monospace; background: #0f0f0f; color: #e0e0e0; padding: 1.5rem; }
.box { background: #1a1a2e; border: 1px solid #2a2a4a; border-radius: 8px;
       padding: 1rem 1.5rem; margin-bottom: 1rem; }
.label { color: #e94560; font-size: 0.85rem; margin-bottom: 0.3rem; }
.value { color: #4ade80; font-size: 1rem; word-break: break-all; }
.dim   { color: #666; }
h2 { color: #e94560; margin-bottom: 1rem; }
</style>""")

print("<h2>Session info (from CGI)</h2>")

print('<div class="box">')
print('<div class="label">HTTP_COOKIE header</div>')
print('<div class="value">{}</div>'.format(cookie_header if cookie_header else '<span class="dim">not sent</span>'))
print('</div>')

print('<div class="box">')
print('<div class="label">Extracted session_id</div>')
if session_id:
    print('<div class="value">{}</div>'.format(session_id))
else:
    print('<div class="value dim">(no session cookie found)</div>')
print('</div>')

print('<div class="box">')
print('<div class="label">All environment variables</div>')
for k in sorted(os.environ):
    print('<div><span style="color:#60a5fa">{}</span> = {}</div>'.format(k, os.environ[k]))
print('</div>')
