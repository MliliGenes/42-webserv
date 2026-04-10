#pragma once

#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <poll.h>
#include "Config.hpp"
#include "../request-response/response/ResponseBuilder.hpp"
#include "../request-response/request/RequestParser.hpp"
#include "../request-response/cookie-session/SessionManager.hpp"
#include "../cgi/include/Cgi.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Connection  –  per-fd state kept alongside the poll array
// ─────────────────────────────────────────────────────────────────────────────
struct Connection
{
    int                 fd;
    const ServerConfig* config;      // which vhost owns this connection
    RequestParser       parser;
    std::string         write_buf;   // serialised response bytes waiting to flush
    bool                keep_alive;

    Connection(int fd_, const ServerConfig* cfg, std::size_t max_body)
        : fd(fd_), config(cfg), parser(max_body), keep_alive(true) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Listener  –  one bound/listening socket per server block
// ─────────────────────────────────────────────────────────────────────────────
struct Listener
{
    int                 fd;
    const ServerConfig* config;
};

// ─────────────────────────────────────────────────────────────────────────────
// Server  –  single-threaded poll(2) event loop
// ─────────────────────────────────────────────────────────────────────────────
class Server
{
public:
    explicit Server(const std::vector<ServerConfig>& servers);
    ~Server();

    void run();  // blocks; returns only on fatal error

private:
    // ── setup ────────────────────────────────────────────────────────────────
    int createListenSocket(const std::string& host, int port);

    // ── poll bookkeeping ─────────────────────────────────────────────────────
    void rebuildPollFds();

    // ── event handlers ───────────────────────────────────────────────────────
    void handleListenerEvent(std::size_t listener_idx);
    void handleClientReadable(std::size_t conn_idx);
    void handleClientWritable(std::size_t conn_idx);
    void closeConnection(std::size_t conn_idx);

    // ── request → response ───────────────────────────────────────────────────
    void     dispatchAndBuffer(Connection& conn);
    Response buildQuickError(int code, const ServerConfig& config);

    // ── members ──────────────────────────────────────────────────────────────
    std::vector<Listener>      listeners_;
    std::vector<Connection*>   conns_;
    std::vector<struct pollfd> pfds_;   // rebuilt when dirty_

    SessionManager  sessions_;
    CgiHandler      cgi_;
    bool            dirty_;
};

#endif
