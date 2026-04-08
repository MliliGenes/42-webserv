#include "Config.hpp"
#include <cstdlib>

Config::Config() {}
Config::~Config() {}

size_t parse_max_body_size(const std::string& size_str) {
    if (size_str.empty())
        return 0;

    size_t multiplier = 1;
    char last_char = size_str[size_str.size() - 1];

    if (last_char == 'K' || last_char == 'k') {
        multiplier = 1024;
    } else if (last_char == 'M' || last_char == 'm') {
        multiplier = 1024 * 1024;
    } else if (last_char == 'G' || last_char == 'g') {
        multiplier = 1024 * 1024 * 1024;
    }

    std::string number_part = (multiplier > 1) ? size_str.substr(0, size_str.size() - 1) : size_str;
    char* end = NULL;
    unsigned long value = std::strtoul(number_part.c_str(), &end, 10);
    if (end == number_part.c_str())
        return 0;

    return static_cast<size_t>(value) * multiplier;
}

std::vector<std::string> get_array_of_strings(TrpJsonArray* arr) {
    std::vector<std::string> result;
    for (size_t i = 0; i < arr->size(); i++) {
        TrpJsonString* str = static_cast<TrpJsonString*>(arr->at(i));
        result.push_back(str->getValue());
    }
    return result;
}

std::map<int, std::string> get_error_pages(TrpJsonObject* errorPagesObj) {
    std::map<int, std::string> errorPages;
    for (JsonObjectMap::const_iterator it = errorPagesObj->begin(); it != errorPagesObj->end(); ++it) {
        int code = std::atoi(it->first.c_str());
        std::string path = static_cast<TrpJsonString*>(it->second)->getValue();
        errorPages[code] = path;
    }
    return errorPages;
}

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

            std::string clientMaxBodySizeStr = static_cast<const TrpJsonString*>(serverBlock->find("client_max_body_size"))->getValue();
            serverConfig.clientMaxBodySize = parse_max_body_size(clientMaxBodySizeStr);

            if (serverBlock->find("server_names")) {
                serverConfig.serverNames = get_array_of_strings(static_cast<TrpJsonArray*>(serverBlock->find("server_names")));
            }

            if (serverBlock->find("index")) {
                serverConfig.index = get_array_of_strings(static_cast<TrpJsonArray*>(serverBlock->find("index")));
            }

            if (serverBlock->find("error_pages")) {
                serverConfig.errorPages = get_error_pages(static_cast<TrpJsonObject*>(serverBlock->find("error_pages")));
            }


            _servers.push_back(serverConfig);
        }
    }
}
