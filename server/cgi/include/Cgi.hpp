#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <csignal>
#include <sstream>
#include <cctype>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

struct CgiRequest
{
    std::string method;
    std::string script;
    std::string interpreter;
    std::string query;
    std::string content_type;
    std::string body;
    std::string cwd;
    std::map<std::string, std::string> extra_env;
    CgiRequest();
};

struct CgiResponse
{
    int status;
    std::string body;
    std::map<std::string, std::string>  headers;

    CgiResponse();
};

class CgiHandler 
{
public:
    CgiHandler();
    // ~CgiHandler(); nzid 5 const later

    bool    execute(const CgiRequest& req, CgiResponse& res, std::string& err, int timeout_sec) const;

private:
    std::vector<std::string> buildEnv(const CgiRequest& req) const;
    bool    runProcess(const CgiRequest& req, const std::vector<std::string>& env, std::string& raw, std::string& err, int timeout_sec) const;
    bool    parseOutput(const std::string& raw, CgiResponse& res, std::string& err) const;
};