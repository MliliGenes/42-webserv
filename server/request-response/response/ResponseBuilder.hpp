#ifndef RESPONSE_BUILDER_HPP
#define RESPONSE_BUILDER_HPP


#include "Request.hpp"
#include "RequestParser.hpp"
#include "Response.hpp"
#include "../server/core/Config.hpp"
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>

class ResponseBuilder {
    public:
        ResponseBuilder();

        Response dispatch(const Request& req, const ServerConfig& config);
        Response buildError(int code, const ServerConfig& config);

    private:
        const LocationConfig* matchRoute(const std::string&  path,
                                    const ServerConfig& config) const;
        std::string statusMessage(int code);

        bool        fileExists(const std::string& path) const;
        std::string readFile(const std::string& path) const;
        std::string sizeToString(std::size_t n);
        std::string resolve_path(std::string root, std::string path);
        Response    buildeResfromOutput(std::string raw, const ServerConfig& config);
        bool        isDirectory(const std::string& path);
        std::string getType(const std::string& path) const;
        Response    listsDirectory(const std::string& fs_path, const std::string& req_path);
        bool writeFile(std::string path, std::string content);

        Response handleGet(const Request&      req,
                       const LocationConfig& route,
                       const ServerConfig&  config);

        Response handlePost(const Request&      req,
                        const LocationConfig& route,
                        const ServerConfig&  config);

        Response handleDelete(const Request&      req,
                          const LocationConfig& route,
                          const ServerConfig&  config);
        

};

#endif