#pragma once

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstddef>   // size_t

// ─────────────────────────────────────────────────────────────────────────────
// LocationConfig  –  one `location /path { … }` block
// ─────────────────────────────────────────────────────────────────────────────
struct LocationConfig
{
    std::string              path;
    std::set<std::string>    methods;
    std::string              root;
    bool                     autoindex;
    std::vector<std::string> index;
    bool                     uploadEnable;
    std::string              uploadStore;

    struct Redirect {
        int         code;
        std::string url;
        bool        enabled;
        Redirect() : code(0), enabled(false) {}
    } redirect;

    std::map<std::string, std::string> cgi;  // extension → interpreter path

    LocationConfig()
        : autoindex(false), uploadEnable(false) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// ServerConfig  –  one `server { … }` block
// ─────────────────────────────────────────────────────────────────────────────
struct ServerConfig
{
    std::string              host;
    int                      port;
    std::vector<std::string> serverNames;
    std::size_t              clientMaxBodySize;
    std::map<int,std::string> errorPages;
    std::string              root;
    std::vector<std::string> index;
    std::vector<LocationConfig> routes;

    ServerConfig()
        : host("0.0.0.0"), port(8080), clientMaxBodySize(1024 * 1024) {}
};

#endif
