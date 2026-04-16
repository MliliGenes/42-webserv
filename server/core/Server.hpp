#pragma once
#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include "Config.hpp"

#include "../request-response/request/RequestParser.hpp"
#include "../request-response/request/Request.hpp"
#include "../request-response/response/Response.hpp"
#include "../request-response/response/ResponseBuilder.hpp"
#include "../request-response/cookie-session/SessionManager.hpp"

// struct Client {
//     int         server_block_index;

//     int         fd;
//     bool        keep_alive;
//     time_t      last_active;

//     std::string req_buf;
//     std::string res_buf;
// };

struct Client {
    int         fd;
    bool        keep_alive;
    time_t      last_active;
    int         server_block_index;

    RequestParser req_parser;   // stateful — owns the parse buffer
    Request       req;          // populated on Complete
    Response      res;          // built on Complete

    // hada for small bodies petits and shit XD 
    std::string   res_buf;      // serialized bytes being drained to socket

    // when the body is fat tbarklah, stream
    std::ifstream res_file;
    off_t         res_file_remaining;

    Client() : fd(-1), keep_alive(true), last_active(0),
            server_block_index(0), res_file_remaining(0) {}
};

class Server {
    
    private:
        const std::vector<ServerConfig>& _configs;
        std::vector<pollfd>              _pollfds;
        std::map<int, Client>            _clients;
        std::map<int, int>               _listeners;

        void _setup_listeners();
        void _handle_accept(int fd);
        void _handle_read(size_t i);
        void _handle_write(size_t i);
        void _close_client(size_t i);
        void _add_fd(int fd, short events);
        bool _is_listener(int fd);
        void _check_timeouts();
        void _close_file(Client& c);

		SessionManager	session;

		ResponseBuilder _res_b;
		cgihandler		_cgi;

    public:
        Server(const Config& cfg);
        void run();

};