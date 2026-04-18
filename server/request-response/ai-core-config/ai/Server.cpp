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
#include <cerrno>

static void set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

Server::Server(const Config& cfg)
    : _configs(cfg.servers()), _res_b(session) {}

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
        if (fd < 0) throw std::runtime_error("socket() failed");

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        set_nonblocking(fd);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(srv.port);

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

        std::cout << "listening on http://" << srv.host << ":" << srv.port
                  << "  fd:" << fd << std::endl;
    }
}

void Server::_handle_accept(int listener_fd) {
    int cfd = accept(listener_fd, NULL, NULL);
    if (cfd < 0) return;
    set_nonblocking(cfd);

    Client c;
    c.fd                 = cfd;
    c.keep_alive         = true;
    c.last_active        = time(NULL);
    c.server_block_index = _listeners[listener_fd];
    c.req_parser.setMaxBodySize(_configs[c.server_block_index].clientMaxBodySize);
    _clients[cfd] = c;
    _add_fd(cfd, POLLIN);

    std::cout << "accepted  fd:" << cfd << std::endl;
}

void Server::_close_client(size_t i) {
    int fd = _pollfds[i].fd;
    std::cout << "closing  fd:" << fd << std::endl;
    close(fd);
    _clients.erase(fd);
    _pollfds[i] = _pollfds.back();
    _pollfds.pop_back();
}

void Server::_handle_read(size_t i) {
    int     fd = _pollfds[i].fd;
    Client& c  = _clients[fd];

    char buf[4096];
    int  n = recv(fd, buf, sizeof(buf), 0);
    if (n < 0 && errno == EAGAIN) return;
    if (n <= 0) { _close_client(i); return; }

    c.last_active = time(NULL);

    // If we already have a pending error response queued, just drain
    // the socket and wait for the write to complete
    if (_pollfds[i].events == POLLOUT) return;  // <-- ADD THIS

    RequestParser::Status status = c.req_parser.feed(buf, n);

    if (status == RequestParser::Incomplete)
        return;

    if (status == RequestParser::Error) {
        c.keep_alive = false;  // <-- FORCE CLOSE on error
        c.res = _res_b.buildError(c.req_parser.getErrorCode(),
                                  _configs[c.server_block_index]);
    } else {
        c.req        = c.req_parser.getRequest();
        c.keep_alive = (c.req.headers.count("connection") &&
                        c.req.headers.at("connection") == "keep-alive");
        c.res = _res_b.dispatch(c.req, _configs[c.server_block_index], _cgi);
    }

    c.res.headers["Connection"] = c.keep_alive ? "keep-alive" : "close";
    if (!c.res.headers.count("Content-Length")) {
        std::ostringstream len;
        len << c.res.body.size();
        c.res.headers["Content-Length"] = len.str();
    }
    c.res_buf = c.res.build();
    _pollfds[i].events = POLLOUT;
}

void Server::_handle_write(size_t i) {
    int     fd = _pollfds[i].fd;
    Client& c  = _clients[fd];

    int n = send(fd, c.res_buf.c_str(), c.res_buf.size(), 0);
    if (n < 0 && errno == EAGAIN) return;
    if (n < 0) { _close_client(i); return; }

    c.res_buf.erase(0, n);
    c.last_active = time(NULL);

    if (!c.res_buf.empty()) return;

    if (c.keep_alive) {
        c.req_parser.reset();
        c.req_parser.setMaxBodySize(_configs[c.server_block_index].clientMaxBodySize);
        c.res_buf.clear();
        _pollfds[i].events = POLLIN;
    } else {
        _close_client(i);
    }
}

void Server::_check_timeouts() {
    time_t now = time(NULL);
    size_t i   = 0;
    while (i < _pollfds.size()) {
        int fd = _pollfds[i].fd;
        if (_is_listener(fd)) { i++; continue; }
        if (now - _clients[fd].last_active > 30) {
            std::cout << "timeout  fd:" << fd << std::endl;
            _close_client(i);
        } else {
            i++;
        }
    }
}

void Server::run() {
    signal(SIGPIPE, SIG_IGN);
    _setup_listeners();

    while (true) {
        int n = poll(&_pollfds[0], _pollfds.size(), 5000);
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
