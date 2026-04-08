#include "Config.hpp"

Config::Config() {}
Config::~Config() {}

Config::Config(ITrpJsonValue* ast) {
    if (!ast)
        return; // exception later

    if (ast->getType() == TRP_ARRAY) {
        TrpJsonArray* servers_array = static_cast<TrpJsonArray*>(ast);
        if (servers_array->size() == 0)
            return; // exception later  
        std::cout << servers_array->size() << " server blocks found in configuration." << std::endl;

        for (size_t i = 0; i < servers_array->size(); i++) {
            TrpJsonObject* serverBlock = static_cast<TrpJsonObject*>(servers_array->at(i))->find("server") ? static_cast<TrpJsonObject*>(static_cast<TrpJsonObject*>(servers_array->at(i))->find("server")) : NULL;

            if (!serverBlock) {
                std::cerr << "Invalid server block at index " << i << "." << std::endl;
                continue; // exception later
            }

            ServerConfig serverConfig;

            serverConfig.host = static_cast<const TrpJsonString*>(serverBlock->find("host"))->getValue();

            serverConfig.port = static_cast<const TrpJsonNumber*>(serverBlock->find("port"))->getValue();

            serverConfig.root = static_cast<const TrpJsonString*>(serverBlock->find("root"))->getValue();

            _servers.push_back(serverConfig);
        }
    }
}
