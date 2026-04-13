#pragma once
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "TrpJson.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>

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
        Redirect() : code(0), enabled(false) {}
    } redirect;

    std::map<std::string, std::string> cgi; // extension -> interpreter

    LocationConfig() : autoindex(false), uploadEnable(false) {}
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

    ServerConfig() : port(80), clientMaxBodySize(1024 * 1024) {}
};

class Config {
private:
    std::vector<ServerConfig> _servers;
    Config();
    Config(const Config&);

public:
    ~Config() {}
    Config(ITrpJsonValue* ast);
    void prettyPrint() const;
    const std::vector<ServerConfig>& servers() const { return _servers; }
};

#endif
