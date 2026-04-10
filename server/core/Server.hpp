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

struct Client {
    int         server_block_index;

    int         fd;
    bool        keep_alive;
    time_t      last_active;

    std::string req_buf;
    std::string res_buf;
};

class Server {
    
    private:
        const std::vector<ServerConfig>& _configs;
        std::vector<pollfd>              _pollfds;
        std::map<int, Client>            _clients;
        std::set<int>                    _listeners; // listener fds

        void _setup_listeners();
        void _handle_accept(int fd);
        void _handle_read(size_t i);
        void _handle_write(size_t i);
        void _close_client(size_t i);
        void _add_fd(int fd, short events);
        bool _is_listener(int fd);
        void _check_timeouts();

		SessionManager	session;

		ResponseBuilder _res_b;
		RequestParser	_req_p;

		Request			_req;
		Response		_res;
		cgihandler		_cgi;

    public:
        Server(const Config& cfg);
        void run();

};