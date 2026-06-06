# Webserv Project Breakdown

## Project Description

`webserv` is a C++98 implementation of a non-blocking HTTP/1.1 web server built for the 42 curriculum.  
The server is configured through JSON files and supports core web server features such as multiple server blocks, request parsing, static file serving, file upload and deletion routes, CGI execution, and custom error handling.

This document breaks the project down into logical modules with clear tasks and prerequisites.

## 📚 Essential Reading

Before diving into the implementation, familiarize yourself with these resources:

- **RFC 1945 (HTTP/1.0)**: A good starting point for understanding the HTTP protocol
- **RFC 2616 (HTTP/1.1)**: More complex, covering keep-alive, chunked encoding, etc.
- **Beej's Guide to Network Programming**: Excellent practical resource for C-based socket programming

---

## Module 1: Server Setup (Socket Programming)

### 🎯 Goal
Create, configure, and "turn on" the server's listening sockets based on your configuration.

### 📋 Tasks

1. **Iterate through configuration**: Loop through the interface:port pairs parsed from your config file

2. **For each pair, perform these steps**:
   - `socket()`: Create a new socket
   - `setsockopt()`: Set the `SO_REUSEADDR` option (crucial for quick restarts)
   - `fcntl()`: Set the socket to non-blocking mode (mandatory requirement)
   - `bind()`: Associate the socket with the specific IP address and port
   - `listen()`: Mark the socket as passive, ready to accept incoming connections

3. **Store all listening file descriptors** in a list or vector for later use

### 🧠 Prerequisites
- **Socket API**: Understanding `socket()`, `setsockopt()`, `bind()`, `listen()`
- **Network Byte Order**: Why and how to use `htons()` (Host to Network Short) for port numbers
- **File Descriptors**: Understand that a socket is just a file descriptor
- **fcntl()**: Specifically how to use `F_SETFL` and `O_NONBLOCK` to make FDs non-blocking

---

## Module 2: The Core Event Loop (I/O Multiplexing)

### 🎯 Goal
Create the main server loop that waits for activity on any socket (listening or client) without blocking.

> ⚠️ **Important**: The subject requires you use only one `poll()` (or equivalent) for all I/O operations. This single call must monitor for both read and write readiness.

### 📋 Tasks

1. **Initialize your multiplexing system**: Choose `poll()`, `select()`, `kqueue()`, or `epoll()` (`poll()` is recommended)

2. **Add listening sockets**: Add all listening sockets from Module 1 to the `poll()` monitoring set, watching for "read" events (new connections)

3. **Create the main loop**: Implement the `while(true)` server loop

4. **Call poll()**: Inside the loop, call `poll()` with your set of file descriptors. This will block until at least one FD is ready

### 🧠 Prerequisites
- **I/O Multiplexing**: Core concept - why wait for one thing when you can wait for anything?
- **poll()**: How to use it with `std::vector<struct pollfd>`, understanding `pollfd` struct (`fd`, `events`, `revents`) and event flags (`POLLIN`, `POLLOUT`)
- **select() (Alternative)**: Understanding `fd_set` and macros `FD_ZERO`, `FD_SET`, `FD_ISSET`. Be aware of `FD_SETSIZE` limitation
- **Event-Driven Architecture**: Grasp the "Reactor" design pattern - your loop reacts to events

---

## Module 3: Connection & Client Management

### 🎯 Goal
Accept new clients and manage their lifecycle (connection, disconnection, errors).

### 📋 Tasks

1. **Iterate poll() results**: After `poll()` returns, loop through monitored FDs

2. **Handle New Connections**:
   - If a listening socket has `POLLIN` event, a new client is trying to connect
   - Call `accept()` on that socket to get a new client socket FD
   - **Crucially**: Set this new client socket to non-blocking mode using `fcntl()`
   - Add this new client FD to your `poll()` monitoring set, watching for `POLLIN`

3. **Handle Client Disconnections**:
   - If `read()` returns 0, the client has closed the connection
   - You must `close()` the socket FD and remove it from `poll()` monitoring

4. **Client State Management**: Track each client with `std::map<int, Client>` where:
   - `int` is the FD
   - `Client` is a class containing:
     - Request buffer (`std::string`)
     - Response buffer (`std::string`)
     - State (e.g., `READING_REQUEST`, `GENERATING_RESPONSE`, `SENDING_RESPONSE`)

### 🧠 Prerequisites
- **accept()**: How to accept new connections and obtain their file descriptors
- **Error Handling**: Understanding `errno`, especially `EAGAIN` and `EWOULDBLOCK` (not errors in non-blocking mode, just "try again later")
- **Finite State Machine (FSM)**: Thinking of client lifecycle as states (READING → WRITING → CLOSING)
- **C++ STL Containers**: `std::map` for fd → Client lookup, `std::vector` for pollfd list

---

## Module 4: HTTP Request Parsing

### 🎯 Goal
Read and understand the HTTP request sent by the client.

### 📋 Tasks

1. **Read Data**: If a client socket has `POLLIN` event, `read()` data from it

2. **Append to Buffer**: Add new data to the Client's internal request buffer

3. **Check for Completion**: Request headers are complete when you find `\r\n\r\n`
   - Handle "partial reads" - you might get `GET /in` in one read and `dex.html ...` in the next

4. **Parse Headers**:
   - **Request Line**: Parse first line (e.g., `GET /page.html HTTP/1.1`) to extract Method, URI, and HTTP Version
   - **Headers**: Parse subsequent lines into `std::map<string, string>`

5. **Parse Body**:
   - For POST methods, check for `Content-Length` header
   - Continue reading until you have exactly `Content-Length` bytes
   - Respect `client_max_body_size` from config (return `413 Payload Too Large` if exceeded)

6. **State Change**: Once full request is received, change Client's state to `GENERATING_RESPONSE`

### 🧠 Prerequisites
- **HTTP Request Format**: Structure: Request-Line → Headers → `\r\n\r\n` → Body (optional)
- **C++ String Manipulation**: `std::string::find()`, `substr()`, `stringstream`
- **Content-Length**: Critical role in determining request end
- **Buffering**: Accumulating data over multiple `read()` calls

---

## Module 5: HTTP Response Generation

### 🎯 Goal
Build the HTTP response string (headers and body) based on the request and config.

### 📋 Tasks

1. **Route Matching**: Find the "location" block in config that best matches the request URI

2. **Method Check**: Verify request method is allowed for that route (return `405 Method Not Allowed` if not)

3. **Action**: Based on route and method, decide what to do:
   - Serve a static file (GET)
   - Generate a directory listing (GET)
   - Handle a file upload (POST)
   - Delete a file (DELETE)
   - Execute a CGI script
   - Perform a redirection
   - Generate an error (404, 403, 500, etc.)

4. **Build Response**: Create response in Client's response buffer:
   - **Status Line**: e.g., `HTTP/1.1 200 OK` or `HTTP/1.1 404 Not Found`
   - **Headers**: Content-Type, Content-Length, Date, Server
   - **Separator**: `\r\n\r\n`
   - **Body**: The actual content

5. **State Change**:
   - Change Client's state to `SENDING_RESPONSE`
   - Modify `poll()` entry: stop watching `POLLIN`, start watching `POLLOUT`

### 🧠 Prerequisites
- **HTTP Response Format**: Status-Line, Headers, Body structure
- **HTTP Status Codes**: Common ones (200 OK, 201 Created, 204 No Content, 301 Redirect, 400 Bad Request, 403 Forbidden, 404 Not Found, 405 Method Not Allowed, 500 Server Error)
- **MIME Types**: Mapping file extensions to Content-Type headers (`.html` → `text/html`, `.css` → `text/css`, `.jpg` → `image/jpeg`)

---

## Module 6: Resource & File Handling

### 🎯 Goal
Implement the logic for GET, POST, and DELETE methods on static files.

### 📋 Tasks

**For GET requests**:
1. Construct full file path using route's `root` directive
2. Use `stat()` to check if path exists (404 if not)
3. If it's a **file**: Read contents into response body
4. If it's a **directory**:
   - Check for default file (e.g., `index.html`)
   - If no default and `directory_listing` is on: Use `opendir()`, `readdir()` to build HTML directory listing
   - If listing is off: Return `403 Forbidden`

**For POST requests** (File Uploads):
1. Check route allows uploads and has `upload_path`
2. Parse request body (possibly `multipart/form-data`) to extract file content
3. Save file to `upload_path`
4. Return `201 Created`

**For DELETE requests**:
1. Construct full file path
2. Use `std::remove()` or `unlink()` to delete file
3. Return `200 OK` or `204 No Content` on success, or `404`/`403` on failure

### 🧠 Prerequisites
- **Filesystem Functions**: `stat()`, `access()` (for checking permissions)
- **Directory Functions**: `opendir()`, `readdir()`, `closedir()`
- **C++ File I/O**: `std::ifstream` (read files), `std::ofstream` (write files)
- **multipart/form-data**: Standard for file uploads (non-trivial parsing required)

---

## Module 7: Sending the Response

### 🎯 Goal
Send the generated response back to the client, respecting non-blocking I/O.

### 📋 Tasks

1. **Check for Readiness**: If client socket has `POLLOUT` event, send buffer has space

2. **send() Data**: Call `send()` with data from Client's response buffer

3. **Handle Partial Sends**:
   - `send()` returns number of bytes actually sent (not always all data)
   - Track progress and send remaining data on next `POLLOUT` event
   - Example: Tried to send 1000 bytes, `send()` returned 250 → send remaining 750 bytes next time

4. **When Fully Sent**:
   - Check request's `Connection` header
   - If **keep-alive**: Reset Client object and change `poll()` monitoring back to `POLLIN`
   - If **close** (or HTTP/1.0): `close()` socket and remove from `poll()`

### 🧠 Prerequisites
- **send()**: How it works in non-blocking mode, handling return value
- **Socket Send Buffers**: Why `send()` might not send all data
- **HTTP Connection Header**: Difference between `keep-alive` and `close` and impact on connection management

---

## Module 8: CGI (Common Gateway Interface)

### 🎯 Goal
Execute an external script (like PHP) and send its output back to the client. **Most complex part.**

### 📋 Tasks

1. **Identify CGI Request**: Matches CGI configuration (e.g., ends in `.php`)

2. **Create Pipes**: Two pipes needed:
   - Server → CGI (stdin)
   - CGI → Server (stdout)
   - Use `pipe()`

3. **fork()**: Create a child process

4. **In the Child Process**:
   - `dup2()` read end of stdin pipe to `STDIN_FILENO`
   - `dup2()` write end of stdout pipe to `STDOUT_FILENO`
   - Close all other file descriptors
   - `chdir()` to correct directory
   - Prepare `execve()` arguments (script path)
   - Prepare environment variables (`REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_LENGTH`, `SCRIPT_FILENAME`)
   - Call `execve()` to replace process with CGI executable (e.g., `php-cgi`)

5. **In the Parent Process**:
   - Close unused pipe ends
   - Get two new FDs: `cgi_stdin_write` and `cgi_stdout_read`
   - Set both pipe FDs to NON-BLOCKING mode (`fcntl()`)
   - Add `cgi_stdout_read` to `poll()` set, watching for `POLLIN`
   - If POST request, add `cgi_stdin_write` to `poll()` set, watching for `POLLOUT`

6. **Integrate with Event Loop**:
   - **Writing to CGI**: When `cgi_stdin_write` is ready (`POLLOUT`), `write()` request body. When done, `close()` to signal EOF
   - **Reading from CGI**: When `cgi_stdout_read` is ready (`POLLIN`), `read()` and buffer output
   - **CGI Done**: When `read()` returns 0, CGI finished. `close()` pipe and call `waitpid()` to prevent zombie process
   - Buffered output is HTTP response (may contain headers - parse them!)

### 🧠 Prerequisites
- **Process Management**: `fork()`, `execve()`, `waitpid()`
- **Inter-Process Communication (IPC)**: `pipe()`, `dup2()`
- **CGI Specification**: Which environment variables to set (read the CGI standard)
- **Advanced Event Loop**: Managing non-socket FDs (pipes) and child processes within same `poll()` loop

---

## Module 9: Testing & Resilience

### 🎯 Goal
Ensure your server is robust and doesn't crash.

### 📋 Tasks

1. **No Crashes**: Server must never crash with bad requests, full memory, or weird disconnects
   - Meticulous error checking on **every** system call

2. **Stress Testing**: Write test script (Python recommended) that sends:
   - Many simultaneous connections
   - Very large requests
   - Partial requests (sent slowly)
   - Malformed requests
   - Clients that connect and disconnect abruptly

3. **Browser Test**: Test with actual browser (Chrome, Firefox)

4. **NGINX Comparison**: Use `telnet` or `curl` to send requests to your server and NGINX, compare responses

### 🧠 Prerequisites
- **telnet / curl**: Sending raw HTTP requests from command line
- **Defensive Programming**: Assume everything can fail. Check return codes for all system calls (`socket`, `bind`, `listen`, `read`, `write`, `send`, `poll`, `fork`, `pipe`, `malloc`/`new`, etc.)
- **Valgrind**: Check for memory leaks

---

## 🎓 Final Notes

This is a challenging but incredibly rewarding project. Take it one module at a time, test thoroughly, and don't hesitate to consult the RFCs and other documentation.

**Good luck!** 🚀