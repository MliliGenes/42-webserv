#include "Config.hpp"
#include "Server.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv <config.json>" << std::endl;
        return 1;
    }

    try {
        ITrpJsonValue* ast = trpJsonParseFile(argv[1]);
        Config config(ast);
        delete ast;

        config.prettyPrint();

        Server server(config);
        server.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
