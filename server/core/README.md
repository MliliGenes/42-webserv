# webserv
> 42 Project — HTTP/1.1 server in C++98 — non-blocking I/O — `poll()`

---

## Table of Contents

1. [Project Structure](#1-project-structure)
2. [How to Build & Run](#2-how-to-build--run)
3. [Configuration File](#3-configuration-file)
4. [Config Parser — `Config`](#4-config-parser--config)
5. [Server Class — `Server`](#5-server-class--server)
6. [The poll() Event Loop](#6-the-poll-event-loop)
7. [Client Lifecycle](#7-client-lifecycle)
8. [Request Inspector (Debug Mode)](#8-request-inspector-debug-mode)
9. [What Is Not Built Yet](#9-what-is-not-built-yet)
10. [Testing](#10-testing)

---

## 1. Project Structure

```
webserv/
├── config/
│   └── configs.json          # server configuration file
├── src/
│   ├── main.cpp              # entry point
│   ├── Config.hpp / Config.cpp      # JSON AST → ServerConfig / LocationConfig
│   ├── Server.hpp / Server.cpp      # TCP server, poll() loop, client I/O
│   └── [json parser files]          # ITrpJsonValue AST implementation
└── Makefile
```

---

## 2. How to Build & Run

```bash
make
./webserv config/configs.json
```

Expected output:
```
2 server blocks found in configuration.
2 valid server blocks loaded into configuration.
listening on 127.0.0.1:8080  fd:4
listening on 0.0.0.0:3000    fd:5
```

Open a browser at `http://127.0.0.1:8080/any/path` to see the request inspector page.

---

## 3. Configuration File

The config is a JSON file. Each object in the root array is one server block.

```json
[
  {
    "host": "127.0.0.1",
    "port": 8080,
    "server_name": ["example.com", "www.example.com"],
    "client_max_body_size": 10485760,
    "root": "/var/www/example",
    "error_pages": {
      "403": "/errors/403.html",
      "404": "/errors/404.html",
      "500": "/errors/500.html"
    },
    "routes": [
      {
        "path": "/",
        "methods": ["GET", "POST"],
        "root": "/var/www/example",
        "autoindex": true,
        "index": ["index.html"]
      },
      {
        "path": "/uploads",
        "methods": ["GET", "POST", "DELETE"],
        "root": "/var/www/example/uploads",
        "autoindex": true,
        "upload_enable": true,
        "upload_store": "/var/www/example/uploads"
      },
      {
        "path": "/cgi-bin",
        "methods": ["GET", "POST"],
        "root": "/var/www/example/cgi-bin",
        "cgi": {
          ".py":  "/usr/bin/python3",
          ".php": "/usr/bin/php-cgi",
          ".js":  "/usr/bin/node",
          ".sh":  "/usr/bin/perl"
        }
      }
    ]
  },
  {
    "host": "0.0.0.0",
    "port": 3000,
    "server_name": ["localhost"],
    "client_max_body_size": 5242880,
    "root": "/var/www/local",
    "index": ["index.html"],
    "error_pages": {
      "404": "/custom_errors/not_found.html",
      "500": "/custom_errors/server_error.html"
    },
    "routes": [
      {
        "path": "/",
        "methods": ["GET"]
      },
      {
        "path": "/files",
        "methods": ["GET", "POST"],
        "root": "/var/www/local/files",
        "autoindex": true,
        "upload_enable": true,
        "upload_store": "/var/www/local/files"
      }
    ]
  }
]
```

### Key fields

| Field | Type | Description |
|---|---|---|
| `host` | string | IP to bind. `"0.0.0.0"` = all interfaces |
| `port` | int | TCP port to listen on |
| `server_name` | array | Matched against the `Host:` header for virtual hosting |
| `client_max_body_size` | int | Max request body in bytes. `10485760` = 10 MB |
| `root` | string | Default document root for this server block |
| `index` | array | Default files to serve for directory requests |
| `error_pages` | object | Map of HTTP status code → custom error page path |
| `routes` | array | List of location blocks (see below) |

### Route fields

| Field | Type | Description |
|---|---|---|
| `path` | string | URI prefix this location matches |
| `methods` | array | Allowed HTTP methods. Anything else → 405 |
| `root` | string | Overrides the server-level root for this route |
| `autoindex` | bool | List directory contents if no index file found |
| `index` | array | Index files to try for this location |
| `upload_enable` | bool | Allow file uploads to this location |
| `upload_store` | string | Directory where uploaded files are saved |
| `cgi` | object | Map of file extension → interpreter binary path |
| `redirect.code` | int | HTTP redirect code (301, 302 …) |
| `redirect.url` | string | Redirect target URL |

---

## 4. Config Parser — `Config`

### Structs

```cpp
struct LocationConfig {
    std::string              path;
    std::set<std::string>    methods;
    std::string              root;
    bool                     autoindex;
    std::vector<std::string> index;
    bool                     uploadEnable;
    std::string              uploadStore;

    struct Redirect {
        int         code;
        std::string url;
        bool        enabled;
    } redirect;

    std::map<std::string, std::string> cgi; // ".py" -> "/usr/bin/python3"
};

struct ServerConfig {
    std::string              host;
    int                      port;
    std::vector<std::string> serverNames;
    size_t                   clientMaxBodySize;
    std::map<int,std::string> errorPages;
    std::string              root;
    std::vector<std::string> index;
    std::vector<LocationConfig> routes;
};
```

### Class

```cpp
class Config {
public:
    Config(ITrpJsonValue* ast);       // walks the JSON AST, fills _servers
    const std::vector<ServerConfig>& servers() const;
    void prettyPrint(void);
};
```

### What it does

`Config` receives the root node of the JSON AST produced by the JSON parser.
It walks every child node that represents a `server {}` block and maps each
JSON field to the corresponding field in `ServerConfig` and `LocationConfig`.

The result is `_servers` — a flat `std::vector<ServerConfig>` that the `Server`
class consumes directly. After `Config` is constructed the JSON AST is no longer
needed.

### Usage

```cpp
// main.cpp
ITrpJsonValue* ast = parse_json_file(argv[1]); // your JSON parser
Config cfg(ast);
cfg.prettyPrint(); // prints all parsed blocks to stdout
```

---

## 5. Server Class — `Server`

### Header

```cpp
struct Client {
    int         fd;
    std::string req_buf;  // accumulates incoming bytes
    std::string res_buf;  // bytes queued to send back
};

class Server {
public:
    Server(const Config& cfg);
    void run();

private:
    const std::vector<ServerConfig>& _configs;
    std::vector<pollfd>              _pollfds;   // watched by poll()
    std::map<int, Client>            _clients;   // fd -> client state
    std::set<int>                    _listeners; // listener fds only

    void _setup_listeners();
    void _handle_accept(int fd);
    void _handle_read(size_t i);
    void _handle_write(size_t i);
    void _close_client(size_t i);
    void _add_fd(int fd, short events);
    bool _is_listener(int fd);
};
```

### _setup_listeners()

Called once at startup. Loops `_configs` and for each `ServerConfig`:

1. `socket(AF_INET, SOCK_STREAM, 0)` — creates a TCP socket fd
2. `setsockopt(..., SO_REUSEADDR, ...)` — allows immediate rebind after restart
3. `fcntl(..., O_NONBLOCK)` — makes the fd non-blocking
4. `bind()` — assigns the configured `host:port` to the socket
5. `listen(fd, 128)` — tells the kernel to queue up to 128 pending connections
6. Inserts fd into `_listeners` and `_pollfds` with `events = POLLIN`

After this function returns, `_pollfds` contains one entry per server block.
The kernel is already accepting TCP handshakes — they queue up until `accept()` is called.

```
Server [0]: 127.0.0.1:8080  fd:4
Server [1]: 0.0.0.0:3000    fd:5
```

---

## 6. The poll() Event Loop

`Server::run()` is an infinite loop. Everything happens inside it.

```
while (true)
    │
    ├── poll(&_pollfds[0], _pollfds.size(), -1)
    │     blocks until ≥1 fd has an event
    │
    └── for each pollfd where revents != 0
          │
          ├── fd is a listener  →  _handle_accept(fd)
          ├── POLLHUP | POLLERR →  _close_client(i)
          ├── POLLIN            →  _handle_read(i)
          └── POLLOUT           →  _handle_write(i)
```

### Why poll() and not select()

`select()` has a hard limit of 1024 fds (`FD_SETSIZE`). `poll()` has no such limit.
For a server that could have hundreds of simultaneous clients, `select()` fails.

### Why -1 timeout

`-1` means block forever until something is ready. This uses zero CPU while idle.
A positive value would be used to implement connection timeouts (not yet built).

### The cached size trick

```cpp
size_t sz = _pollfds.size(); // cache BEFORE the loop
for (size_t i = 0; i < sz; i++) {
```

`_handle_accept()` calls `_pollfds.push_back()` which can reallocate the vector.
If the loop used `_pollfds.size()` directly, the reallocation would invalidate
the cached pointer and the new element could be processed in the same iteration
before it is ready. Caching `sz` prevents this.

### POLLIN vs POLLOUT

A client fd starts with `events = POLLIN` (waiting for request data).
The moment a complete request is received and a response is built, `events` is
flipped to `POLLOUT` (waiting for the kernel send buffer to have space).
Once the response is fully sent, `events` flips back to `POLLIN`.

**Never watch POLLOUT when there is nothing to send.** The kernel send buffer is
almost always writable, so poll() would return immediately every iteration and
burn 100% CPU in a spin loop.

---

## 7. Client Lifecycle

```
accept()
   │
   ▼  events = POLLIN
READING ──── recv() partial ──────────────────► READING (stays, waits)
   │
   │  recv() + request_complete() == true
   ▼
PROCESSING ── build_html_response() ──────────► fills res_buf
   │
   │  events = POLLOUT
   ▼
WRITING ───── send() partial ─────────────────► WRITING (stays, next POLLOUT)
   │
   │  res_buf.empty()
   ▼
close_client()  →  close(fd)  →  erase from _clients  →  erase from _pollfds
```

### _handle_accept

Calls `accept()` to dequeue one connection from the kernel's accept backlog.
Sets the new fd to `O_NONBLOCK`, creates a `Client` struct, adds it to
`_clients` and `_pollfds` with `events = POLLIN`.

### _handle_read

```cpp
int n = recv(fd, buf, sizeof(buf), 0);
if (n < 0 && errno == EAGAIN) return;   // no data yet, come back
if (n <= 0) { _close_client(i); return; } // EOF or error
_clients[fd].req_buf.append(buf, n);
if (request_complete(_clients[fd].req_buf)) {
    // build response, flip to POLLOUT
}
```

`EAGAIN` must be checked **before** `n <= 0`. When the fd is non-blocking,
`recv()` returns `-1` with `errno == EAGAIN` to mean "no data available right now,
try again later." This is not an error — it is the normal non-blocking behavior.
Treating it as an error would close the client connection immediately.

### _handle_write

```cpp
int n = send(fd, c.res_buf.c_str(), c.res_buf.size(), 0);
c.res_buf.erase(0, n); // remove ONLY the bytes that were sent
if (c.res_buf.empty())
    _pollfds[i].events = POLLIN; // done, wait for next request
```

`send()` may accept fewer bytes than requested (partial send). `erase(0, n)`
removes only what was accepted. The remaining bytes stay in `res_buf` and are
sent on the next `POLLOUT` event. Never assume `n == res_buf.size()`.

### _close_client — swap-and-pop

```cpp
void Server::_close_client(size_t i) {
    int fd = _pollfds[i].fd;
    close(fd);
    _clients.erase(fd);
    _pollfds[i] = _pollfds.back(); // overwrite slot with last element
    _pollfds.pop_back();           // remove the (now duplicate) last element
}
```

Erasing from the middle of a vector is O(n) because every element after the
removed one must shift left. Swap-and-pop replaces the removed slot with the
last element and pops the back — O(1). The caller must do `i--` after calling
this so the loop re-examines the swapped-in element.

### request_complete

```cpp
static bool request_complete(const std::string& buf) {
    size_t header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;  // headers not done yet

    size_t cl = buf.find("Content-Length:");
    if (cl != std::string::npos) {
        int body_len = atoi(buf.c_str() + cl + 15);
        return (int)(buf.size() - header_end - 4) >= body_len;
    }
    return true; // no body (GET / HEAD / DELETE)
}
```

HTTP headers are terminated by `\r\n\r\n`. For requests with a body (POST),
`Content-Length` tells us how many body bytes to expect. The function returns
`false` until all of them have arrived in `req_buf`.

---

## 8. Request Inspector (Debug Mode)

While the HTTP response builder is not yet implemented, the server responds to
every request with a debug HTML page showing:

- The full request line (`METHOD PATH HTTP/VERSION`)
- Every HTTP header the client sent, in a two-column table
- The raw body if any was sent

This is served as a valid `HTTP/1.1 200 OK` response with correct
`Content-Type: text/html` and `Content-Length` headers.

Open any path in a browser:
```
http://127.0.0.1:8080/
http://127.0.0.1:8080/hello/world
http://127.0.0.1:8080/test?foo=bar
```

The terminal logs every request line:
```
fd:6  GET / HTTP/1.1
fd:6  GET /favicon.ico HTTP/1.1
fd:7  GET /hello/world HTTP/1.1
```

---

## 9. What Is Not Built Yet

The following will be implemented next, in order:

| # | What | Notes |
|---|---|---|
| 1 | `request_complete()` for chunked encoding | `Transfer-Encoding: chunked` bodies |
| 2 | HTTP request parser | `HttpRequest` struct — partner's code |
| 3 | Virtual host routing | Match `Host:` header to `ServerConfig` via `serverNames` |
| 4 | Location matching | Longest-prefix match on URI against `routes` |
| 5 | Static file serving | `GET` → read file → respond with correct `Content-Type` |
| 6 | Directory listing | `autoindex: on` → generate HTML file list |
| 7 | POST / file upload | Write body to `uploadStore` directory |
| 8 | DELETE | Remove file, respond `204 No Content` |
| 9 | Custom error pages | Look up `errorPages` map, serve file or default HTML |
| 10 | CGI execution | `fork` + `execve`, pipe stdin/stdout through `pollfds` |
| 11 | Connection timeouts | Evict clients silent longer than N seconds |
| 12 | Keep-alive | Reset client to `READING` after response instead of closing |

---

## 10. Testing

### nc (netcat) — raw TCP

```bash
# connect and type manually — stays open, echoes nothing (echo removed)
nc 127.0.0.1 8080

# send a raw GET request
printf "GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc 127.0.0.1 8080
```

### curl

```bash
# basic GET
curl -v http://127.0.0.1:8080/

# POST with body
curl -v -X POST -d "hello=world" http://127.0.0.1:8080/submit

# custom Host header (virtual hosting test)
curl -v -H "Host: example.com" http://127.0.0.1:8080/

# large body (tests Content-Length buffering)
curl -v -X POST --data-binary @/etc/hosts http://127.0.0.1:8080/upload
```

### Browser

Open `http://127.0.0.1:8080/any/path` — the request inspector page shows
exactly what your HTTP parser will receive as input.

### Concurrent connections

```bash
# open 3 terminals, connect simultaneously
# terminal 1
nc 127.0.0.1 8080
# terminal 2
nc 127.0.0.1 8080
# terminal 3
nc localhost 3000
```

All three should be accepted and tracked independently.
None should block the others.

### Siege (stress test — run last)

```bash
siege -c 50 -t 10s http://127.0.0.1:8080/
```

Server must not crash, must not leak fds, must not block.

---

## Signal handling

```cpp
signal(SIGPIPE, SIG_IGN);
```

`SIGPIPE` is raised when `send()` writes to a socket whose remote end has
already closed. The default action is to terminate the process — fatal for a
server. `SIG_IGN` makes `send()` return `-1` with `errno == EPIPE` instead,
which the write handler treats as a normal close.

## SO_REUSEADDR

```cpp
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

After a process exits, TCP connections linger in `TIME_WAIT` state for up to
60 seconds. Without `SO_REUSEADDR`, restarting the server during this window
causes `bind()` to return `EADDRINUSE`. This option tells the kernel to allow
rebinding the port immediately.

## O_NONBLOCK

```cpp
fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
```

Set on **every** fd — both listener sockets and accepted client sockets.
In blocking mode, `recv()` waits indefinitely for data. If one slow client
triggers a blocking `recv()`, every other connected client is frozen until
data arrives. Non-blocking mode makes `recv()` return `-1` with `errno == EAGAIN`
immediately when no data is available, so the loop continues to the next fd.
