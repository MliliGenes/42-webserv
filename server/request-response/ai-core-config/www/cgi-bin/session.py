# Example using CGI / web request environment

import os

cookies = os.environ.get("HTTP_COOKIE", "")

if "session_id=" in cookies:
    print("we are inside session")
else:
    print("we are not bro")