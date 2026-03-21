#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

#endif
