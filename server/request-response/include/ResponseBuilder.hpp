#ifndef RESPONSE_BUILDER_HPP
#define RESPONSE_BUILDER_HPP


#include "Request.hpp"
#include "../../cgi/include/Cgi.hpp"
#include "Response.hpp"
#include "../../core/Config.hpp"
#include "SessionManager.hpp"
#include "RequestParser.hpp"
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>


class ResponseBuilder {
    public:
        ResponseBuilder(SessionManager& sessions);

        
        Response dispatch(const Request& req, const ServerConfig& config, cgihandler& cgi);
        Response buildError(int code, const ServerConfig& config);

    private:
        const LocationConfig* matchRoute(const std::string&  path,
            const ServerConfig& config) const;
            
        std::string statusMessage(int code);
        std::string sizeToString(std::size_t n);
        std::string getType(const std::string& path) const;
        std::string readFile(const std::string& path) const;

        std::string resolve_path(std::string root, std::string path, std::string rout_path);
        bool        deleteFile(std::string path);
        bool        isDirectory(const std::string& path);
        bool        fileExists(const std::string& path) const;
        bool        writeFile(std::string path, std::string content);

        Response    listsDirectory(const std::string& fs_path, const std::string& req_path);

        
        Response    handleGet(const Request&      req,
                       const LocationConfig& route,
                       const ServerConfig&  config, cgihandler& cgi);

        Response    handlePost(const Request&      req,
                        const LocationConfig& route,
                        const ServerConfig&  config, cgihandler& cgi);

        Response    handleDelete(const Request&      req,
                          const LocationConfig& route,
                          const ServerConfig&  config, cgihandler& cgi);
        //bonus

        SessionManager& sessions_; //this attribut where i store the sessions
        void        applySessionCookie(const Request& req, Response& res);
        

};

#endif