#pragma once
#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include "../include/RequestParser.hpp"
#include "../include/ResponseBuilder.hpp"
#include "../include/SessionManager.hpp"
#include "../../cgi/include/Cgi.hpp"

#include <vector>
#include <map>
#include <poll.h>
#include <ctime>
#include <sstream>

struct Client {
    int              fd;
    bool             keep_alive;
    time_t           last_active;
    size_t           server_block_index;
    RequestParser    req_parser;
    Request          req;
    Response         res;
    std::string      res_buf;
};

class Server {
public:
    Server(const Config& cfg);
    void run();

private:
    const std::vector<ServerConfig>& _configs;
    std::map<int, size_t>            _listeners;   // fd -> server block index
    std::map<int, Client>            _clients;
    std::vector<pollfd>              _pollfds;

    SessionManager   session;
    ResponseBuilder  _res_b;
    CgiHandler       _cgi;

    void _setup_listeners();
    void _add_fd(int fd, short events);
    bool _is_listener(int fd);
    void _handle_accept(int listener_fd);
    void _handle_read(size_t i);
    void _handle_write(size_t i);
    void _close_client(size_t i);
    void _check_timeouts();
};

#endif
