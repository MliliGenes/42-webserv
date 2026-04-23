#pragma once
#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include <sys/types.h>
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
    struct CgiJob {
        bool        active;
        pid_t       pid;
        int         in_fd;
        int         out_fd;
        size_t      body_sent;
        std::string raw;
        time_t      start;
        int         timeout_sec;
        bool        exited;
        int         exit_status;
        CgiRequest  req;

        CgiJob() : active(false), pid(-1), in_fd(-1), out_fd(-1), body_sent(0),
                   start(0), timeout_sec(5), exited(false), exit_status(0) {}
    };

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

    CgiJob        cgi;

    Client() : fd(-1), keep_alive(true), last_active(0),
            server_block_index(0), res_file_remaining(0) {}
};

class Server {
    
    private:
        const std::vector<ServerConfig>& _configs;
        std::vector<pollfd>              _pollfds;
        std::map<int, Client*>            _clients;
        std::map<int, int>               _listeners;
        struct CgiFdInfo {
            int  client_fd;
            bool is_stdin;
        };
        std::map<int, CgiFdInfo>          _cgi_fds;

        void _setup_listeners();
        void _handle_accept(int fd);
        bool _handle_read(size_t i);
        bool _handle_write(size_t i);
        void _handle_cgi_event(size_t i);
        void _close_client(size_t i);
        void _add_fd(int fd, short events);
        void _add_cgi_fd(int fd, short events, int client_fd, bool is_stdin);
        bool _is_listener(int fd);
        bool _is_cgi_fd(int fd) const;
        void _check_timeouts();
        void _check_cgi_jobs();
        void _close_file(Client* c);
        void _cleanup_cgi(Client* c);
        void _invalidate_pollfd_by_fd(int fd);
        void _compact_pollfds();
        void _set_client_events(int fd, short events);
        void _finalize_cgi(Client* c, const ServerConfig& cfg);

		SessionManager	session;

		ResponseBuilder _res_b;
		cgihandler		_cgi;

    public:
        Server(const Config& cfg);
        ~Server();
        void run();

};