#include "Server.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <cstring>
#include <csignal>

static void sig_noop(int) {}

int main(int argc, char* argv[])
{
    // ── config file ──────────────────────────────────────────────────────────
    std::string config_path = "webserv.conf";
    if (argc >= 2)
        config_path = argv[1];

    std::vector<ServerConfig> servers;
    try
    {
        servers = ConfigParser::parseFile(config_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[main] Config error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "[main] Loaded " << servers.size() << " server block(s)\n";

    // ── signals ──────────────────────────────────────────────────────────────
    ::signal(SIGPIPE, SIG_IGN);   // ignore broken pipe; we handle write errors inline

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_noop;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGINT,  &sa, NULL);
    ::sigaction(SIGTERM, &sa, NULL);

    // ── run ──────────────────────────────────────────────────────────────────
    Server srv(servers);
    srv.run();

    return 0;
}
