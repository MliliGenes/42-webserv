#include "Config.hpp"
#include <cstdlib>
#include <stdexcept>

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

LocationConfig get_location(TrpJsonObject* locationBlock) {
    LocationConfig locConfig;
    locConfig.path = static_cast<const TrpJsonString*>(locationBlock->find("path"))->getValue();
    locConfig.autoindex = locationBlock->find("autoindex") ? static_cast<const TrpJsonBool*>(locationBlock->find("autoindex"))->getValue() : false;
    locConfig.uploadEnable = locationBlock->find("upload_enable") ? static_cast<const TrpJsonBool*>(locationBlock->find("upload_enable"))->getValue() : false;
    locConfig.uploadStore = locationBlock->find("upload_store") ? static_cast<const TrpJsonString*>(locationBlock->find("upload_store"))->getValue() : "";

    if (locationBlock->find("methods")) {
        std::vector<std::string> methods = get_array_of_strings(static_cast<TrpJsonArray*>(locationBlock->find("methods")));
        locConfig.methods = std::set<std::string>(methods.begin(), methods.end());
    }

    if (locationBlock->find("root")) {
        locConfig.root = static_cast<const TrpJsonString*>(locationBlock->find("root"))->getValue();
    }

    if (locationBlock->find("index")) {
        locConfig.index = get_array_of_strings(static_cast<TrpJsonArray*>(locationBlock->find("index")));
    }
    
    if (locationBlock->find("cgi")) {
        TrpJsonObject* cgiObj = static_cast<TrpJsonObject*>(locationBlock->find("cgi"));
        for (JsonObjectMap::const_iterator it = cgiObj->begin(); it != cgiObj->end(); ++it) {
            std::string extension = it->first;
            std::string interpreter = static_cast<TrpJsonString*>(it->second)->getValue();
            locConfig.cgi[extension] = interpreter;
        }
    }

    return locConfig;
}

std::vector<LocationConfig> get_locations(TrpJsonArray* locationsArray) {
    std::vector<LocationConfig> locations;
    for (size_t i = 0; i < locationsArray->size(); i++) {
        TrpJsonObject* locationBlock = static_cast<TrpJsonObject*>(locationsArray->at(i));
        LocationConfig locConfig = get_location(locationBlock);
        locations.push_back(locConfig);

    }
    return locations;
}

Config::Config(ITrpJsonValue* ast) {
    if (!ast)
        throw std::runtime_error("Bad file."); // exception later

    if (ast->getType() == TRP_ARRAY) {
        TrpJsonArray* servers_array = static_cast<TrpJsonArray*>(ast);
        if (servers_array->size() == 0)
            throw std::runtime_error("No configs provided."); 
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

            if (serverBlock->find("server_name")) {
                serverConfig.serverNames = get_array_of_strings(static_cast<TrpJsonArray*>(serverBlock->find("server_name")));
            }

            if (serverBlock->find("index")) {
                serverConfig.index = get_array_of_strings(static_cast<TrpJsonArray*>(serverBlock->find("index")));
            }

            if (serverBlock->find("error_pages")) {
                serverConfig.errorPages = get_error_pages(static_cast<TrpJsonObject*>(serverBlock->find("error_pages")));
            }

            if (serverBlock->find("locations")) {
                serverConfig.routes = get_locations(static_cast<TrpJsonArray*>(serverBlock->find("locations")));
            }

            _servers.push_back(serverConfig);
        }
        std::cout << _servers.size() << " valid server blocks loaded into configuration." << std::endl;
    }
}

const std::vector<ServerConfig>& Config::servers(void) const {
    return _servers;
}

void Config::prettyPrint(void) {
    for (size_t i = 0; i < _servers.size(); i++) {
        const ServerConfig& srv = _servers[i];
        std::cout << "Server [" << i << "]:\n";
        std::cout << "  host:              " << srv.host << "\n";
        std::cout << "  port:              " << srv.port << "\n";
        std::cout << "  clientMaxBodySize: " << srv.clientMaxBodySize << "\n";
        std::cout << "  root:              " << srv.root << "\n";

        std::cout << "  serverNames:       ";
        for (size_t j = 0; j < srv.serverNames.size(); j++)
            std::cout << srv.serverNames[j] << (j + 1 < srv.serverNames.size() ? ", " : "");
        std::cout << "\n";

        std::cout << "  index:             ";
        for (size_t j = 0; j < srv.index.size(); j++)
            std::cout << srv.index[j] << (j + 1 < srv.index.size() ? ", " : "");
        std::cout << "\n";

        if (!srv.errorPages.empty()) {
            std::cout << "  errorPages:\n";
            for (std::map<int, std::string>::const_iterator it = srv.errorPages.begin(); it != srv.errorPages.end(); ++it)
                std::cout << "    " << it->first << " -> " << it->second << "\n";
        }

        for (size_t j = 0; j < srv.routes.size(); j++) {
            const LocationConfig& loc = srv.routes[j];
            std::cout << "  Location [" << j << "]: " << loc.path << "\n";
            std::cout << "    root:         " << loc.root << "\n";
            std::cout << "    autoindex:    " << (loc.autoindex ? "on" : "off") << "\n";
            std::cout << "    uploadEnable: " << (loc.uploadEnable ? "on" : "off") << "\n";
            std::cout << "    uploadStore:  " << loc.uploadStore << "\n";

            std::cout << "    methods:      ";
            for (std::set<std::string>::const_iterator it = loc.methods.begin(); it != loc.methods.end(); ++it)
                std::cout << (it != loc.methods.begin() ? ", " : "") << *it;
            std::cout << "\n";

            std::cout << "    index:        ";
            for (size_t k = 0; k < loc.index.size(); k++)
                std::cout << loc.index[k] << (k + 1 < loc.index.size() ? ", " : "");
            std::cout << "\n";

            if (loc.redirect.enabled)
                std::cout << "    redirect:     " << loc.redirect.code << " -> " << loc.redirect.url << "\n";

            if (!loc.cgi.empty()) {
                std::cout << "    cgi:\n";
                for (std::map<std::string, std::string>::const_iterator it = loc.cgi.begin(); it != loc.cgi.end(); ++it)
                    std::cout << "      " << it->first << " -> " << it->second << "\n";
            }
        }
        std::cout << "\n";
    }
}