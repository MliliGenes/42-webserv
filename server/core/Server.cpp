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

Server::Server(const Config& cfg) : _configs(cfg.servers()), _res_b(session) {
}

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

        _listeners[fd] = i;
        _add_fd(fd, POLLIN);

        std::cout << "listening on http://" << srv.host << ":" << srv.port << "  fd:" << fd << std::endl;
    }
}

void Server::_handle_accept(int listener_fd) {
    int cfd = accept(listener_fd, NULL, NULL);
    if (cfd < 0) return;                  // EAGAIN or error — skip
    set_nonblocking(cfd);

    Client c;
    c.fd = cfd;
    c.keep_alive = true;
    c.last_active = time(NULL);
    c.server_block_index = _listeners[listener_fd];
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

std::string parse_header(const std::string& buf, const std::string& key) {
    size_t pos = buf.find(key + ":");
    if (pos == std::string::npos) return "";
    pos += key.size() + 1;
    while (pos < buf.size() && buf[pos] == ' ') pos++;
    size_t end = buf.find("\r\n", pos);
    return buf.substr(pos, end - pos);
}


void Server::_handle_read(size_t i) {
    int fd = _pollfds[i].fd;
    char buf[4096];
    int n = recv(fd, buf, sizeof(buf), 0);

    if (n < 0 && errno == EAGAIN) return;
    if (n <= 0) { _close_client(i); return; }

    _clients[fd].req_buf.append(buf, n);
    _clients[fd].last_active = time(NULL);

    // log
    size_t line_end = _clients[fd].req_buf.find("\r\n");
    std::cout << "fd:" << fd << " at server " << _clients[fd].server_block_index << "  " << _clients[fd].req_buf.substr(0, line_end) << std::endl;

    _clients[fd].keep_alive = is_keep_alive(_clients[fd].req_buf);
    _req_p.reset();
    RequestParser::Status parse_status = _req_p.feed(_clients[fd].req_buf.c_str(), _clients[fd].req_buf.size());

    if (parse_status == RequestParser::Error) {
        _res = _res_b.buildError(_req_p.getErrorCode(), _configs[_clients[fd].server_block_index]);
    } else if (parse_status == RequestParser::Complete) {
        _req = _req_p.getRequest();
        _res = _res_b.dispatch(_req, _configs[_clients[fd].server_block_index], _cgi);
    } else if (parse_status == RequestParser::Incomplete) {
        std::cout << "incomplete request from fd:" << fd << std::endl;
        return;
    }

    _res.headers["Connection"] = _clients[fd].keep_alive ? "keep-alive" : "close";
    if (_res.headers.find("Content-Length") == _res.headers.end() &&
        _res.headers.find("content-length") == _res.headers.end()) {
        std::ostringstream len;
        len << _res.body.size();
        _res.headers["Content-Length"] = len.str();
    }

    _clients[fd].res_buf = _res.build();
    _clients[fd].req_buf.clear();
    _pollfds[i].events = POLLOUT;
}

void Server::_handle_write(size_t i) {
    int fd = _pollfds[i].fd;
    Client& c = _clients[fd];

    int n = send(fd, c.res_buf.c_str(), c.res_buf.size(), 0);
    if (n < 0 && errno == EAGAIN) return;
    if (n < 0) { 
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

        if (n == 0) {
            continue;
        }

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