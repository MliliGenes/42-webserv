#ifndef RESPONSE_BUILDER_HPP
#define RESPONSE_BUILDER_HPP


#include "Request.hpp"
#include "RequestParser.hpp"
#include "Response.hpp"
#include "../server/core/Config.hpp"
#include "../cookie-session/SessionManager.hpp"
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>


class ResponseBuilder {
    public:
        ResponseBuilder(SessionManager& sessions);

        
        Response dispatch(const Request& req, const ServerConfig& config);
        Response buildError(int code, const ServerConfig& config);

    private:
        const LocationConfig* matchRoute(const std::string&  path,
            const ServerConfig& config) const;
            
        std::string statusMessage(int code);
        std::string sizeToString(std::size_t n);
        std::string getType(const std::string& path) const;
        std::string readFile(const std::string& path) const;

        std::string resolve_path(std::string root, std::string path);
        bool        deleteFile(std::string path);
        bool        isDirectory(const std::string& path);
        bool        fileExists(const std::string& path) const;
        bool        writeFile(std::string path, std::string content);
        Response    buildeResfromOutput(std::string raw, const ServerConfig& config);

        Response    listsDirectory(const std::string& fs_path, const std::string& req_path);

        
        Response    handleGet(const Request&      req,
                       const LocationConfig& route,
                       const ServerConfig&  config);

        Response    handlePost(const Request&      req,
                        const LocationConfig& route,
                        const ServerConfig&  config);

        Response    handleDelete(const Request&      req,
                          const LocationConfig& route,
                          const ServerConfig&  config);
        //bonus

        SessionManager& sessions_; //this attribut where i store the sessions
        void        applySessionCookie(const Request& req, Response& res);
        

};

#endif