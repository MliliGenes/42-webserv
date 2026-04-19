#pragma once
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <csignal>
#include <sstream>
#include <cctype>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <exception>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

struct CgiRequest
{
    std::string method;
    std::string script_path;
    std::string script_name;
    std::string path_info;
    std::string interpreter_path;
    std::string query_string;
    std::string content_type;
    std::string server_name;
    std::string server_port;
    std::string server_protocol;
    std::string body;
    std::string working_directory;
    std::map<std::string, std::string> extra_env;
    CgiRequest();
};

struct CgiResponse
{
    int status_code;
    std::string body;
    std::vector<std::pair<std::string, std::string> > headers;
    CgiResponse();
};

struct CgiProcess
{
    pid_t pid;
    int   in_fd;
    int   out_fd;
    CgiProcess() : pid(-1), in_fd(-1), out_fd(-1) {}
};

class CgiHandler 
{
public:
    CgiHandler();
    // ~CgiHandler(); nzid 5 const later

    bool    execute(const CgiRequest& req, CgiResponse& res, std::string& err, int timeout_sec = 5) const;
    bool    spawn(const CgiRequest& req, CgiProcess& proc, std::string& err) const;
    bool    parseOutput(const std::string& raw, CgiResponse& res, std::string& err) const;

private:
    std::vector<std::string> buildEnv(const CgiRequest& req) const;
    bool    runProcess(const CgiRequest& req, const std::vector<std::string>& env, std::string& raw, std::string& err, int timeout_sec) const;
};

typedef CgiRequest  cgirequest;
typedef CgiResponse cgiresponse;
typedef CgiHandler  cgihandler;