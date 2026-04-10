#include "Server.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

// ─────────────────────────────────────────────────────────────────────────────
// File-scope utilities
// ─────────────────────────────────────────────────────────────────────────────

static void set_nonblock(int fd)
{
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void set_reuse(int fd)
{
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor  –  bind one listening socket per ServerConfig
// ─────────────────────────────────────────────────────────────────────────────
Server::Server(const std::vector<ServerConfig>& servers)
    : sessions_(), cgi_(), dirty_(true)
{
    for (std::size_t i = 0; i < servers.size(); ++i)
    {
        const ServerConfig& cfg = servers[i];
        int lfd = createListenSocket(cfg.host, cfg.port);
        if (lfd < 0)
        {
            std::cerr << "[Server] Cannot bind "
                      << cfg.host << ":" << cfg.port
                      << " — " << std::strerror(errno) << "\n";
            continue;
        }

        Listener l;
        l.fd     = lfd;
        l.config = &servers[i];   // pointer into caller-owned vector; must outlive Server
        listeners_.push_back(l);

        std::cout << "[Server] Listening on "
                  << cfg.host << ":" << cfg.port << "\n";
    }

    if (listeners_.empty())
    {
        std::cerr << "[Server] No listeners could be created. Aborting.\n";
        std::exit(1);
    }
}

Server::~Server()
{
    for (std::size_t i = 0; i < conns_.size(); ++i)
    {
        ::close(conns_[i]->fd);
        delete conns_[i];
    }
    for (std::size_t i = 0; i < listeners_.size(); ++i)
        ::close(listeners_[i].fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// createListenSocket
// ─────────────────────────────────────────────────────────────────────────────
int Server::createListenSocket(const std::string& host, int port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    set_reuse(fd);
    set_nonblock(fd);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));

    if (host.empty() || host == "0.0.0.0")
        addr.sin_addr.s_addr = INADDR_ANY;
    else if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
    {
        ::close(fd); return -1;
    }

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        ::close(fd); return -1;
    }
    if (::listen(fd, SOMAXCONN) != 0)
    {
        ::close(fd); return -1;
    }
    return fd;
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildPollFds
//
// pfds_ layout:
//   [0 … listeners_.size()-1]   → listening sockets (POLLIN only)
//   [listeners_.size() … end]   → client sockets    (POLLIN always,
//                                                     POLLOUT only if write_buf non-empty)
// ─────────────────────────────────────────────────────────────────────────────
void Server::rebuildPollFds()
{
    pfds_.clear();
    pfds_.reserve(listeners_.size() + conns_.size());

    for (std::size_t i = 0; i < listeners_.size(); ++i)
    {
        struct pollfd pfd;
        pfd.fd      = listeners_[i].fd;
        pfd.events  = POLLIN;
        pfd.revents = 0;
        pfds_.push_back(pfd);
    }

    for (std::size_t i = 0; i < conns_.size(); ++i)
    {
        struct pollfd pfd;
        pfd.fd     = conns_[i]->fd;
        pfd.events = POLLIN;
        if (!conns_[i]->write_buf.empty())
            pfd.events |= POLLOUT;
        pfd.revents = 0;
        pfds_.push_back(pfd);
    }

    dirty_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// run  –  the event loop
// ─────────────────────────────────────────────────────────────────────────────
void Server::run()
{
    while (true)
    {
        if (dirty_)
            rebuildPollFds();

        int ready = ::poll(&pfds_[0], static_cast<nfds_t>(pfds_.size()), -1);
        if (ready < 0)
        {
            if (errno == EINTR) continue;
            std::cerr << "[Server] poll: " << std::strerror(errno) << "\n";
            return;
        }

        // ── listeners ────────────────────────────────────────────────────────
        for (std::size_t i = 0; i < listeners_.size(); ++i)
        {
            if (pfds_[i].revents & POLLIN)
                handleListenerEvent(i);
        }

        // ── clients (iterate with index; closeConnection shrinks conns_) ─────
        std::size_t base = listeners_.size();
        std::size_t i    = 0;
        while (i < conns_.size())
        {
            // pfds_ may be stale if dirty_ was set mid-loop after a close,
            // but we only read pfds_[base+i] if base+i < pfds_.size().
            if (base + i >= pfds_.size())
                break;

            struct pollfd& pfd = pfds_[base + i];
            int saved_fd       = conns_[i]->fd;

            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                closeConnection(i);
                continue;  // i not incremented; next conn_ now sits at same index
            }
            if (pfd.revents & POLLIN)
                handleClientReadable(i);

            // handleClientReadable may have closed the connection (EOF or error)
            // detect that by comparing the fd still at index i
            if (i >= conns_.size() || conns_[i]->fd != saved_fd)
                continue;

            if (pfd.revents & POLLOUT)
                handleClientWritable(i);

            // handleClientWritable may close too
            if (i >= conns_.size() || conns_[i]->fd != saved_fd)
                continue;

            ++i;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// handleListenerEvent  –  accept a new client
// ─────────────────────────────────────────────────────────────────────────────
void Server::handleListenerEvent(std::size_t idx)
{
    struct sockaddr_in cli;
    socklen_t          cli_len = sizeof(cli);

    int cfd = ::accept(listeners_[idx].fd,
                       reinterpret_cast<struct sockaddr*>(&cli), &cli_len);
    if (cfd < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            std::cerr << "[Server] accept: " << std::strerror(errno) << "\n";
        return;
    }

    set_nonblock(cfd);

    const ServerConfig* cfg      = listeners_[idx].config;
    std::size_t         max_body = cfg->clientMaxBodySize
                                   ? cfg->clientMaxBodySize
                                   : 1024 * 1024;

    conns_.push_back(new Connection(cfd, cfg, max_body));
    dirty_ = true;

    std::cout << "[Server] Accepted fd=" << cfd << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// handleClientReadable  –  read bytes, feed the parser
// ─────────────────────────────────────────────────────────────────────────────
void Server::handleClientReadable(std::size_t idx)
{
    Connection& conn = *conns_[idx];
    char        buf[8192];

    ssize_t n = ::read(conn.fd, buf, sizeof(buf));

    if (n == 0)                                          // peer closed
    {
        closeConnection(idx);
        return;
    }
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        closeConnection(idx);
        return;
    }

    RequestParser::Status status =
        conn.parser.feed(buf, static_cast<std::size_t>(n));

    if (status == RequestParser::Error)
    {
        // Parser rejected the request (400 / 405 / 413 / 505).
        // Build an error response, buffer it, then close after flush.
        Response err_res         = buildQuickError(conn.parser.getErrorCode(),
                                                   *conn.config);
        err_res.headers["Connection"] = "close";
        conn.write_buf += err_res.build();
        conn.keep_alive = false;
        dirty_ = true;
        return;
    }

    if (status == RequestParser::Complete)
        dispatchAndBuffer(conn);
    // Incomplete → keep reading on the next wake-up
}

// ─────────────────────────────────────────────────────────────────────────────
// dispatchAndBuffer  –  route the parsed request → buffer the response
// ─────────────────────────────────────────────────────────────────────────────
void Server::dispatchAndBuffer(Connection& conn)
{
    const Request& req = conn.parser.getRequest();

    // ── decide keep-alive ────────────────────────────────────────────────────
    {
        std::map<std::string, std::string>::const_iterator it =
            req.headers.find("connection");

        if (it != req.headers.end())
        {
            // tolower the value for comparison
            std::string val = it->second;
            for (std::size_t i = 0; i < val.size(); ++i)
                val[i] = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(val[i])));
            conn.keep_alive = (val.find("keep-alive") != std::string::npos);
        }
        else
        {
            // HTTP/1.1 default is keep-alive; HTTP/1.0 default is close
            conn.keep_alive = (req.version == "HTTP/1.1");
        }
    }

    // ── build response ───────────────────────────────────────────────────────
    ResponseBuilder builder(sessions_);
    Response        res = builder.dispatch(req, *conn.config, cgi_);

    res.headers["Connection"] = conn.keep_alive ? "keep-alive" : "close";

    conn.write_buf += res.build();
    conn.parser.reset();
    dirty_ = true;  // need POLLOUT on this fd
}

// ─────────────────────────────────────────────────────────────────────────────
// handleClientWritable  –  drain write_buf
// ─────────────────────────────────────────────────────────────────────────────
void Server::handleClientWritable(std::size_t idx)
{
    Connection& conn = *conns_[idx];
    if (conn.write_buf.empty()) return;

    ssize_t n = ::write(conn.fd,
                        conn.write_buf.c_str(),
                        conn.write_buf.size());
    if (n > 0)
        conn.write_buf.erase(0, static_cast<std::size_t>(n));
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        closeConnection(idx);
        return;
    }

    if (conn.write_buf.empty() && !conn.keep_alive)
        closeConnection(idx);
    else
        dirty_ = true;  // trigger POLLOUT flag recalculation on next rebuild
}

// ─────────────────────────────────────────────────────────────────────────────
// closeConnection
// ─────────────────────────────────────────────────────────────────────────────
void Server::closeConnection(std::size_t idx)
{
    std::cout << "[Server] Closed fd=" << conns_[idx]->fd << "\n";
    ::close(conns_[idx]->fd);
    delete conns_[idx];
    conns_.erase(conns_.begin() + static_cast<std::ptrdiff_t>(idx));
    dirty_ = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildQuickError  –  error before a route is matched (parser-level failures)
// ─────────────────────────────────────────────────────────────────────────────
Response Server::buildQuickError(int code, const ServerConfig& config)
{
    ResponseBuilder builder(sessions_);
    return builder.buildError(code, config);
}
