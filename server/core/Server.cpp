// Server.cpp
#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

Server::Server(const Config& cfg) : _configs(cfg.servers()), _res_b(session) {
}

static void set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

static void safe_close(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void Server::_add_fd(int fd, short events) {
    pollfd pfd = {};
    pfd.fd     = fd;
    pfd.events = events;
    _pollfds.push_back(pfd);
}

void Server::_add_cgi_fd(int fd, short events, int client_fd, bool is_stdin) {
    if (fd < 0)
        return;
    _add_fd(fd, events);
    CgiFdInfo info;
    info.client_fd = client_fd;
    info.is_stdin = is_stdin;
    _cgi_fds[fd] = info;
}

bool Server::_is_listener(int fd) {
    return _listeners.count(fd) > 0;
}

bool Server::_is_cgi_fd(int fd) const {
    return _cgi_fds.count(fd) > 0;
}

void Server::_invalidate_pollfd_by_fd(int fd) {
    for (size_t i = 0; i < _pollfds.size(); ++i) {
        if (_pollfds[i].fd == fd) {
            _pollfds[i].fd = -1;
            _pollfds[i].events = 0;
            _pollfds[i].revents = 0;
            break;
        }
    }
}

void Server::_compact_pollfds() {
    size_t i = 0;
    while (i < _pollfds.size()) {
        if (_pollfds[i].fd < 0) {
            _pollfds[i] = _pollfds.back();
            _pollfds.pop_back();
            continue;
        }
        ++i;
    }
}

void Server::_set_client_events(int fd, short events) {
    for (size_t i = 0; i < _pollfds.size(); ++i) {
        if (_pollfds[i].fd == fd) {
            _pollfds[i].events = events;
            return;
        }
    }
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

void Server::_close_file(Client& c) {
    if (c.res_file.is_open()) {
        c.res_file.close();
        c.res_file_remaining = 0;
    }
}

void Server::_handle_accept(int listener_fd) {
    int cfd = accept(listener_fd, NULL, NULL);
    if (cfd < 0) return;
    set_nonblocking(cfd);

    Client& c = _clients[cfd];
    c.fd                 = cfd;
    c.keep_alive         = true;
    c.last_active        = time(NULL);
    c.server_block_index = _listeners[listener_fd];
    c.req_parser.setMaxBodySize(_configs[c.server_block_index].clientMaxBodySize);

    _add_fd(cfd, POLLIN);
    std::cout << "accepted  fd:" << cfd << std::endl;
}

void Server::_close_client(size_t i) {
    int fd = _pollfds[i].fd;
    std::cout << "closing  fd:" << fd << std::endl;
    if (_clients.count(fd))
        _cleanup_cgi(_clients[fd]);
    close(fd);
    _clients.erase(fd);
    _pollfds[i] = _pollfds.back();
    _pollfds.pop_back();
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

void Server::_handle_read(size_t i) {
    int fd = _pollfds[i].fd;
    Client& c = _clients[fd];

    if (c.cgi.active)
        return;

    char buf[4096];
    int n = recv(fd, buf, sizeof(buf), 0);
    if (n < 0 && errno == EAGAIN) return;
    if (n <= 0) { _close_client(i); return; }

    c.last_active = time(NULL);
    // adnan nadi dar chunks XD, w ga3 ma9alali
    RequestParser::Status status = c.req_parser.feed(buf, n);

    if (status == RequestParser::Incomplete) return;

    if (status == RequestParser::Error) {
        c.res = _res_b.buildError(c.req_parser.getErrorCode(), _configs[c.server_block_index]);
    } else {
        c.req = c.req_parser.getRequest();
        c.keep_alive = (c.req.headers.count("connection") &&
                        c.req.headers.at("connection") == "keep-alive");

        CgiRequest cgireq;
        bool is_cgi = false;
        c.res = _res_b.dispatch(c.req, _configs[c.server_block_index], _cgi, &cgireq, &is_cgi);
        if (is_cgi)
        {
            std::string err;
            CgiProcess proc;
            if (!_cgi.spawn(cgireq, proc, err))
            {
                c.res = _res_b.buildError(500, _configs[c.server_block_index]);
                _res_b.applySessionCookieIfNeeded(c.req, c.res);
                c.res.headers["Connection"] = c.keep_alive ? "keep-alive" : "close";
                if (!c.res.headers.count("Content-Length")) {
                    std::ostringstream len;
                    len << c.res.body.size();
                    c.res.headers["Content-Length"] = len.str();
                }
                c.res_buf = c.res.build();
                _pollfds[i].events = POLLOUT;
                return;
            }
            c.cgi.active = true;
            c.cgi.pid = proc.pid;
            c.cgi.in_fd = proc.in_fd;
            c.cgi.out_fd = proc.out_fd;
            c.cgi.body_sent = 0;
            c.cgi.raw.clear();
            c.cgi.start = time(NULL);
            c.cgi.exited = false;
            c.cgi.exit_status = 0;
            c.cgi.req = cgireq;

            if (!cgireq.body.empty())
                _add_cgi_fd(proc.in_fd, POLLOUT, c.fd, true);
            else
                safe_close(c.cgi.in_fd);

            _add_cgi_fd(proc.out_fd, POLLIN | POLLHUP, c.fd, false);
            _pollfds[i].events = 0;
            return;
        }
    }

    c.res.headers["Connection"] = c.keep_alive ? "keep-alive" : "close";
    if (!c.res.headers.count("Content-Length")) {
        std::ostringstream len;
        len << c.res.body.size();
        c.res.headers["Content-Length"] = len.str();
    }

    if (!c.res.body_path.empty()) {
        c.res_file.open(c.res.body_path.c_str(), std::ios::binary);
        if (!c.res_file.is_open()) {
            c.res = _res_b.buildError(404, _configs[c.server_block_index]);
            c.res_buf = c.res.build();
        } else {
            c.res_file.seekg(0, std::ios::end);
            c.res_file_remaining = c.res_file.tellg();
            c.res_file.seekg(0, std::ios::beg);
            c.res_buf = c.res.build_headers();
        }
    } else {
        c.res_buf = c.res.build();
    }
    _pollfds[i].events = POLLOUT;
}

// TODO: change this bullshit to the new logic
void Server::_handle_write(size_t i) {
    int fd = _pollfds[i].fd;
    Client& c = _clients[fd];

    if (c.cgi.active)
        return;

    if (!c.res_buf.empty()) {
        int n = send(fd, c.res_buf.c_str(), c.res_buf.size(), 0);
        if (n < 0 && errno == EAGAIN) return;
        if (n < 0) { _close_client(i); return; }
        c.res_buf.erase(0, n);
        c.last_active = time(NULL);
        if (!c.res_buf.empty()) return;
    }

    #define FILE_CHUNK 1000000

    if (c.res_file.is_open()) {
        if (c.res_file_remaining > 0) {
            char chunk[FILE_CHUNK];
            std::streamsize to_read = std::min((off_t)FILE_CHUNK, c.res_file_remaining);

            c.res_file.read(chunk, to_read);
            std::streamsize r = c.res_file.gcount();
            if (r <= 0) { _close_file(c); _close_client(i); return; }

            ssize_t sent = send(fd, chunk, r, 0);
            if (sent < 0 && errno == EAGAIN) {
                c.res_file.seekg(-r, std::ios::cur);
                return;
            }
            if (sent < 0) { _close_file(c); _close_client(i); return; }

            if (sent < r)
                c.res_file.seekg(-(r - sent), std::ios::cur);

            c.res_file_remaining -= sent;
            c.last_active = time(NULL);
            return;
        }
        _close_file(c);
    }

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

void Server::_handle_cgi_event(size_t i) {
    int fd = _pollfds[i].fd;
    std::map<int, CgiFdInfo>::iterator it = _cgi_fds.find(fd);
    if (it == _cgi_fds.end())
        return;

    std::map<int, Client>::iterator cit = _clients.find(it->second.client_fd);
    if (cit == _clients.end()) {
        _cgi_fds.erase(it);
        _invalidate_pollfd_by_fd(fd);
        safe_close(fd);
        return;
    }

    Client& c = cit->second;
    if (!c.cgi.active) {
        _cgi_fds.erase(it);
        _invalidate_pollfd_by_fd(fd);
        safe_close(fd);
        return;
    }

    if (it->second.is_stdin)
    {
        if (c.cgi.body_sent >= c.cgi.req.body.size()) {
            _cgi_fds.erase(it);
            _invalidate_pollfd_by_fd(fd);
            safe_close(c.cgi.in_fd);
            return;
        }
        size_t remaining = c.cgi.req.body.size() - c.cgi.body_sent;
        size_t chunk = remaining > 4096 ? 4096 : remaining;
        ssize_t w = ::write(fd, c.cgi.req.body.c_str() + c.cgi.body_sent, chunk);
        if (w > 0)
            c.cgi.body_sent += (size_t)w;
        if (w > 0)
            c.last_active = time(NULL);
        if (w <= 0 || c.cgi.body_sent >= c.cgi.req.body.size()) {
            _cgi_fds.erase(it);
            _invalidate_pollfd_by_fd(fd);
            safe_close(c.cgi.in_fd);
            return;
        }
    }
    else
    {
        char buf[4096];
        ssize_t r = ::read(fd, buf, sizeof(buf));
        if (r > 0) {
            c.cgi.raw.append(buf, (size_t)r);
            c.last_active = time(NULL);
            return;
        }
        _cgi_fds.erase(it);
        _invalidate_pollfd_by_fd(fd);
        safe_close(c.cgi.out_fd);
        return;
    }
}

void Server::_cleanup_cgi(Client& c) {
    if (c.cgi.pid > 0) {
        ::kill(c.cgi.pid, SIGKILL);
        ::waitpid(c.cgi.pid, NULL, WNOHANG);
    }
    if (c.cgi.in_fd >= 0) {
        _cgi_fds.erase(c.cgi.in_fd);
        _invalidate_pollfd_by_fd(c.cgi.in_fd);
        safe_close(c.cgi.in_fd);
    }
    if (c.cgi.out_fd >= 0) {
        _cgi_fds.erase(c.cgi.out_fd);
        _invalidate_pollfd_by_fd(c.cgi.out_fd);
        safe_close(c.cgi.out_fd);
    }
    c.cgi.active = false;
    c.cgi.exited = false;
    c.cgi.pid = -1;
}

void Server::_finalize_cgi(Client& c, const ServerConfig& cfg) {
    bool ok = true;
    if (WIFSIGNALED(c.cgi.exit_status))
        ok = false;
    if (WIFEXITED(c.cgi.exit_status) && WEXITSTATUS(c.cgi.exit_status) != 0 && c.cgi.raw.empty())
        ok = false;

    if (ok) {
        CgiResponse cgires;
        std::string err;
        if (!_cgi.parseOutput(c.cgi.raw, cgires, err))
            ok = false;
        else
            c.res = _res_b.buildFromCgi(cgires);
    }

    if (!ok)
        c.res = _res_b.buildError(500, cfg);

    _res_b.applySessionCookieIfNeeded(c.req, c.res);
    c.res.headers["Connection"] = c.keep_alive ? "keep-alive" : "close";
    if (!c.res.headers.count("Content-Length")) {
        std::ostringstream len;
        len << c.res.body.size();
        c.res.headers["Content-Length"] = len.str();
    }
    c.res_buf = c.res.build();
    _set_client_events(c.fd, POLLOUT);

    c.cgi.active = false;
    c.cgi.exited = false;
    c.cgi.pid = -1;
}

void Server::_check_cgi_jobs() {
    time_t now = time(NULL);
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client& c = it->second;
        if (!c.cgi.active)
            continue;

        if (c.cgi.timeout_sec > 0 && (now - c.cgi.start) > c.cgi.timeout_sec) {
            _cleanup_cgi(c);
            c.res = _res_b.buildError(500, _configs[c.server_block_index]);
            _res_b.applySessionCookieIfNeeded(c.req, c.res);
            c.res.headers["Connection"] = c.keep_alive ? "keep-alive" : "close";
            if (!c.res.headers.count("Content-Length")) {
                std::ostringstream len;
                len << c.res.body.size();
                c.res.headers["Content-Length"] = len.str();
            }
            c.res_buf = c.res.build();
            _set_client_events(c.fd, POLLOUT);
            continue;
        }

        if (!c.cgi.exited) {
            pid_t r = ::waitpid(c.cgi.pid, &c.cgi.exit_status, WNOHANG);
            if (r == c.cgi.pid)
                c.cgi.exited = true;
        }
        if (c.cgi.exited && c.cgi.out_fd < 0)
            _finalize_cgi(c, _configs[c.server_block_index]);
    }
}

void Server::_check_timeouts() {
    time_t now = time(NULL);
    size_t i = 0;
    while (i < _pollfds.size()) {
        int fd = _pollfds[i].fd;
        if (_is_listener(fd) || _is_cgi_fd(fd)) { i++; continue; }
        std::map<int, Client>::iterator it = _clients.find(fd);
        if (it == _clients.end()) { i++; continue; }
        if (now - it->second.last_active > 30) {
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
        int n = poll(&_pollfds[0], _pollfds.size(), 100);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        _check_timeouts();
        _check_cgi_jobs();

        if (n == 0) {
            _compact_pollfds();
            continue;
        }

        size_t sz = _pollfds.size();
        for (size_t i = 0; i < sz; i++) {
            if (_pollfds[i].fd < 0) continue;
            if (_pollfds[i].revents == 0) continue;

            int fd = _pollfds[i].fd;

            if (_is_cgi_fd(fd)) {
                _handle_cgi_event(i);
                continue;
            }

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
        _compact_pollfds();
    }
}