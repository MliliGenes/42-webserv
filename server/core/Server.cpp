// Server.cpp
#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

Server::Server(const Config& cfg) : _configs(cfg.servers()) {}

static void set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void Server::_add_fd(int fd, short events) {
    pollfd pfd = {};
    pfd.fd     = fd;
    pfd.events = events;
    _pollfds.push_back(pfd);
}

bool Server::_is_listener(int fd) {
    return _listeners.count(fd) > 0;
}

void Server::_setup_listeners() {
    for (size_t i = 0; i < _configs.size(); i++) {
        const ServerConfig& srv = _configs[i];

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("socket() failed");

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        set_nonblocking(fd);
    
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(srv.port);

        // your host field is a string like "127.0.0.1" or "0.0.0.0"
        if (srv.host == "0.0.0.0" || srv.host.empty())
            addr.sin_addr.s_addr = INADDR_ANY;
        else
            addr.sin_addr.s_addr = inet_addr(srv.host.c_str());

        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed on port " + srv.host);
        if (listen(fd, 128) < 0)
            throw std::runtime_error("listen() failed");

        _listeners.insert(fd);
        _add_fd(fd, POLLIN);

        std::cout << "listening on " << srv.host << ":" << srv.port << "  fd:" << fd << std::endl;
    }
}

void Server::_handle_accept(int listener_fd) {

    int server_index = -1;
    int i = 0;

    for (std::set<int>::iterator it = _listeners.begin(); it != _listeners.end(); ++it, ++i) {
        if (*it == listener_fd) {
            server_index = i;
            break;
        }
    }

    int cfd = accept(listener_fd, NULL, NULL);
    if (cfd < 0) return;                  // EAGAIN or error — skip
    set_nonblocking(cfd);

    Client c;
    c.fd = cfd;
    c.keep_alive = true;
    c.last_active = time(NULL);
    c.server_block_index = server_index;
    _clients[cfd] = c;
    _add_fd(cfd, POLLIN);

    std::cout << "accepted fd:" << cfd << std::endl;
}

void Server::_close_client(size_t i) {
    int fd = _pollfds[i].fd;
    std::cout << "closing  fd:" << fd << std::endl;
    close(fd);
    _clients.erase(fd);
    _pollfds[i] = _pollfds.back();
    _pollfds.pop_back();
}

// always bosting my emotions
static bool is_keep_alive(const std::string& req_buf) {
    // HTTP/1.1 defaults to keep-alive unless client says close
    bool is_1_1 = req_buf.find("HTTP/1.1") != std::string::npos;

    size_t pos = req_buf.find("Connection:");
    if (pos == std::string::npos)
        return is_1_1; // no header → follow version default

    // read the value
    pos += 11;
    while (pos < req_buf.size() && req_buf[pos] == ' ') pos++;
    std::string val = req_buf.substr(pos, req_buf.find("\r\n", pos) - pos);

    if (val.find("close") != std::string::npos)    return false;
    if (val.find("keep-alive") != std::string::npos) return true;
    return is_1_1;
}

// put this helper above _handle_read
bool request_complete(const std::string& buf) {
    size_t header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;

    size_t cl = buf.find("Content-Length:");
    if (cl != std::string::npos) {
        int body_len = atoi(buf.c_str() + cl + 15);
        return (int)(buf.size() - header_end - 4) >= body_len;
    }
    return true;
}

static std::string parse_header(const std::string& buf, const std::string& key) {
    size_t pos = buf.find(key + ":");
    if (pos == std::string::npos) return "";
    pos += key.size() + 1;
    while (pos < buf.size() && buf[pos] == ' ') pos++;
    size_t end = buf.find("\r\n", pos);
    return buf.substr(pos, end - pos);
}

std::string build_json_response(const std::string& req_buf) {
    // parse request line
    size_t line_end = req_buf.find("\r\n");
    std::string request_line = req_buf.substr(0, line_end);

    std::string method, path, version;
    size_t s1 = request_line.find(' ');
    size_t s2 = request_line.find(' ', s1 + 1);
    if (s1 != std::string::npos && s2 != std::string::npos) {
        method  = request_line.substr(0, s1);
        path    = request_line.substr(s1 + 1, s2 - s1 - 1);
        version = request_line.substr(s2 + 1);
    }

    // pull useful headers
    std::string host         = parse_header(req_buf, "Host");
    std::string user_agent   = parse_header(req_buf, "User-Agent");
    std::string content_type = parse_header(req_buf, "Content-Type");
    std::string accept       = parse_header(req_buf, "Accept");
    std::string connection   = parse_header(req_buf, "Connection");

    // extract body if any
    std::string body;
    size_t header_end = req_buf.find("\r\n\r\n");
    if (header_end != std::string::npos)
        body = req_buf.substr(header_end + 4);

    // escape quotes in strings just in case
    // build json body
    std::string json =
        "{\n"
        "  \"method\": \""       + method       + "\",\n"
        "  \"path\": \""         + path         + "\",\n"
        "  \"httpVersion\": \""  + version      + "\",\n"
        "  \"host\": \""         + host         + "\",\n"
        "  \"userAgent\": \""    + user_agent   + "\",\n"
        "  \"accept\": \""       + accept       + "\",\n"
        "  \"contentType\": \""  + content_type + "\",\n"
        "  \"connection\": \""   + connection   + "\",\n"
        "  \"bodyLength\": "     + (body.empty() ? "0" : std::to_string((int)body.size())) + ",\n"
        "  \"body\": \""         + (body.empty() ? "" : body) + "\"\n"
        "}";

    // build raw HTTP response
    std::string content_length = std::to_string(json.size());
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + content_length + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + json;

    return response;
}

static std::string build_html_response(const std::string& req_buf) {
    size_t line_end = req_buf.find("\r\n");
    std::string request_line = req_buf.substr(0, line_end);

    std::string method, path, version;
    size_t s1 = request_line.find(' ');
    size_t s2 = request_line.find(' ', s1 + 1);
    if (s1 != std::string::npos && s2 != std::string::npos) {
        method  = request_line.substr(0, s1);
        path    = request_line.substr(s1 + 1, s2 - s1 - 1);
        version = request_line.substr(s2 + 1);
    }

    std::string host       = parse_header(req_buf, "Host");
    std::string user_agent = parse_header(req_buf, "User-Agent");
    std::string accept     = parse_header(req_buf, "Accept");
    std::string connection = parse_header(req_buf, "Connection");
    std::string encoding   = parse_header(req_buf, "Accept-Encoding");
    std::string language   = parse_header(req_buf, "Accept-Language");

    std::string body;
    size_t header_end = req_buf.find("\r\n\r\n");
    if (header_end != std::string::npos)
        body = req_buf.substr(header_end + 4);

    // collect ALL headers into a table
    std::string headers_rows;
    std::string headers_section = req_buf.substr(line_end + 2);
    size_t hend = headers_section.find("\r\n\r\n");
    if (hend != std::string::npos) headers_section = headers_section.substr(0, hend);

    size_t pos = 0;
    while (pos < headers_section.size()) {
        size_t nl = headers_section.find("\r\n", pos);
        if (nl == std::string::npos) nl = headers_section.size();
        std::string hline = headers_section.substr(pos, nl - pos);
        size_t colon = hline.find(':');
        if (colon != std::string::npos) {
            std::string hkey = hline.substr(0, colon);
            std::string hval = hline.substr(colon + 1);
            while (!hval.empty() && hval[0] == ' ') hval.erase(0, 1);
            headers_rows +=
                "<tr><td>" + hkey + "</td><td>" + hval + "</td></tr>\n";
        }
        pos = nl + 2;
    }

    std::string body_section;
    if (!body.empty()) {
        body_section =
            "<section>"
            "<h2>body</h2>"
            "<pre>" + body + "</pre>"
            "</section>";
    }

    std::string html =
        "<!DOCTYPE html>"
        "<html lang='en'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>webserv — " + method + " " + path + "</title>"
        "<style>"
        "  *{box-sizing:border-box;margin:0;padding:0}"
        "  body{font-family:monospace;background:#0f0f0f;color:#d4d4d4;padding:2rem}"
        "  h1{font-size:1.1rem;color:#4ec9b0;margin-bottom:2rem;letter-spacing:.05em}"
        "  h2{font-size:.75rem;color:#858585;text-transform:uppercase;letter-spacing:.1em;margin-bottom:.75rem}"
        "  section{margin-bottom:2rem}"
        "  .request-line{background:#1e1e1e;border-left:3px solid #4ec9b0;"
        "    padding:.75rem 1rem;margin-bottom:2rem;font-size:1rem}"
        "  .method{color:#569cd6;margin-right:.75rem}"
        "  .path{color:#dcdcaa;margin-right:.75rem}"
        "  .version{color:#858585}"
        "  table{width:100%;border-collapse:collapse;font-size:.85rem}"
        "  td{padding:.4rem .6rem;border-bottom:1px solid #1e1e1e;vertical-align:top}"
        "  tr:last-child td{border-bottom:none}"
        "  td:first-child{color:#9cdcfe;white-space:nowrap;width:220px}"
        "  td:last-child{color:#ce9178;word-break:break-all}"
        "  pre{background:#1e1e1e;padding:1rem;font-size:.85rem;color:#b5cea8;"
        "    white-space:pre-wrap;word-break:break-word;border-left:3px solid #569cd6}"
        "  .empty{color:#555;font-size:.85rem;font-style:italic}"
        "</style>"
        "</head>"
        "<body>"
        "<h1>webserv / request inspector</h1>"

        "<div class='request-line'>"
        "  <span class='method'>"  + method  + "</span>"
        "  <span class='path'>"    + path    + "</span>"
        "  <span class='version'>" + version + "</span>"
        "</div>"

        "<section>"
        "<h2>headers</h2>"
        "<table>" + headers_rows + "</table>"
        "</section>"

        + body_section +

        (body.empty()
            ? "<section><h2>body</h2><p class='empty'>no body</p></section>"
            : "") +

        "</body></html>";

    std::string len = std::to_string(html.size());
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " + len + "\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        + html;
}

void Server::_handle_read(size_t i) {
    int fd = _pollfds[i].fd;
    char buf[4096];
    int n = recv(fd, buf, sizeof(buf), 0);

    if (n < 0 && errno == EAGAIN) return;
    if (n <= 0) { _close_client(i); return; }

    _clients[fd].req_buf.append(buf, n);
    _clients[fd].last_active = time(NULL);

    if (!request_complete(_clients[fd].req_buf)) return; // wait for more data

    // log
    size_t line_end = _clients[fd].req_buf.find("\r\n");
    std::cout << "fd:" << fd << " at server " << _clients[fd].server_block_index << "  " << _clients[fd].req_buf.substr(0, line_end) << std::endl;

    _clients[fd].keep_alive = is_keep_alive(_clients[fd].req_buf);
    _clients[fd].res_buf    = build_html_response(_clients[fd].req_buf);
    _clients[fd].req_buf.clear();
    _pollfds[i].events = POLLOUT;
}

void Server::_handle_write(size_t i) {
    int fd = _pollfds[i].fd;
    Client& c = _clients[fd];

    int n = send(fd, c.res_buf.c_str(), c.res_buf.size(), 0);
    if (n < 0 && errno == EAGAIN) return;
    if (n < 0) { 
        std::cout << "keep alive is off, send failed" << std::endl;
        _close_client(i); return;
    }

    c.res_buf.erase(0, n);
    c.last_active = time(NULL);

    // still have more to pollout go back to loop
    if (!c.res_buf.empty()) return;

    // response fully sent — branch here
    if (c.keep_alive) {
        c.req_buf.clear();          // throw away the old request
        c.res_buf.clear();          // already empty but be explicit
        _pollfds[i].events = POLLIN; // wait for the NEXT request
    } else {
        std::cout << "keep alive is off after send" << std::endl;
        _close_client(i);           // Connection: close → tear down
    }
}

void Server::_check_timeouts() {
    time_t now = time(NULL);
    size_t i = 0;
    while (i < _pollfds.size()) {
        int fd = _pollfds[i].fd;
        if (_is_listener(fd)) { i++; continue; }
        if (now - _clients[fd].last_active > 30) {
            std::cout << "timeout  fd:" << fd << std::endl;
            _close_client(i); // no i++ — swap-and-pop put a new fd at index i
        } else {
            i++;
        }
    }
}

void Server::run() {
    signal(SIGPIPE, SIG_IGN);
    _setup_listeners();

    while (true) {
        int n = poll(&_pollfds[0], _pollfds.size(), -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        _check_timeouts();

        if (n == 0) continue;

        size_t sz = _pollfds.size();
        for (size_t i = 0; i < sz; i++) {
            if (_pollfds[i].revents == 0) continue;

            int fd = _pollfds[i].fd;

            if (_is_listener(fd)) {
                _handle_accept(fd);
                continue;
            }
            if (_pollfds[i].revents & (POLLHUP | POLLERR)) {
                _close_client(i--); sz--;
                continue;
            }
            if (_pollfds[i].revents & POLLIN)
                _handle_read(i);

            if (_clients.count(fd) && i < _pollfds.size() && _pollfds[i].fd == fd)
                if (_pollfds[i].revents & POLLOUT)
                    _handle_write(i);
        }
    }
}