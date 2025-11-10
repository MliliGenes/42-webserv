#pragma once

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "../include/TrpJson.hpp"
#include <set>

// since i am forcing the user to add an array it should be always an array
// * this class should flatten the data to a easy to access structs

struct LocationConfig {
    std::string path;
    std::set<std::string> methods;
    std::string root;
    bool autoindex;
    std::vector<std::string> index;
    bool uploadEnable;
    std::string uploadStore;
    
    struct Redirect {
        int code;
        std::string url;
        bool enabled;
        Redirect(): code(0), enabled(false) {}
    } redirect;

    std::map<std::string, std::string> cgi; // extension -> interpreter path
};

struct ServerConfig {
    std::string host;
    int port;
    std::vector<std::string> serverNames;
    size_t clientMaxBodySize;
    std::map<int, std::string> errorPages;
    std::string root;
    std::vector<std::string> index;
    std::vector<LocationConfig> routes;
};

class Config {
    private:
        std::vector<ServerConfig> _servers;

    public:
        Config(const TrpJsonArray* ast);
        const std::vector<ServerConfig>& servers() const;
};


#endif