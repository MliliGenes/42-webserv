#include "../include/Cgi.hpp"

static void safe_close(int& fd)
{
    if (fd >= 0) { ::close(fd); fd = -1; }
}

static std::string itoa_str(size_t n)
{
    std::ostringstream ss;
    ss << n;
    return ss.str();
}

static std::string str_trim(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace((unsigned char)s[b])) ++b;
    size_t e = s.size();
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

static std::string str_lower(const std::string& s)
{
    std::string r(s);
    for (size_t i = 0; i < r.size(); ++i)
        r[i] = (char)std::tolower((unsigned char)r[i]);
    return r;
}

CgiRequest::CgiRequest() : method("GET") {}

CgiResponse::CgiResponse() : status_code(200) {}

CgiHandler::CgiHandler() {}

bool CgiHandler::execute(const CgiRequest& req, CgiResponse& res, std::string& err, int timeout_sec) const
{
    try
    {
        err.clear();
        res = CgiResponse();

        std::string raw;
        std::vector<std::string> env = buildEnv(req);

        if (!runProcess(req, env, raw, err, timeout_sec))
            return false;
        if (!parseOutput(raw, res, err))
            return false;
        return true;
    }
    catch (const std::exception& e)
    {
        err = e.what();
    }
    catch (...)
    {
        err = "unknown CGI error";
    }
    return false;
}

std::vector<std::string> CgiHandler::buildEnv(const CgiRequest& req) const
{
    std::vector<std::string> env;

    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=" + (req.server_protocol.empty() ? "HTTP/1.1" : req.server_protocol));
    env.push_back("REQUEST_METHOD=" + (req.method.empty() ? "GET" : req.method));
    env.push_back("PATH_INFO=" + req.path_info);
    env.push_back("SCRIPT_FILENAME=" + req.script_path);
    env.push_back("SCRIPT_NAME=" + req.script_name);
    env.push_back("QUERY_STRING=" + req.query_string);
    env.push_back("CONTENT_LENGTH=" + itoa_str(req.body.size()));
    env.push_back("SERVER_NAME=" + req.server_name);
    env.push_back("SERVER_PORT=" + req.server_port);
    env.push_back("REDIRECT_STATUS=200");

    if (!req.content_type.empty())
        env.push_back("CONTENT_TYPE=" + req.content_type);

    for (std::map<std::string, std::string>::const_iterator it = req.extra_env.begin();
         it != req.extra_env.end(); ++it)
        env.push_back(it->first + "=" + it->second);

    return env;
}

bool CgiHandler::spawn(const CgiRequest& req, CgiProcess& proc, std::string& err) const
{
    std::vector<std::string> env = buildEnv(req);
    std::vector<char*>       envp;
    int in_p[2]  = {-1, -1};
    int out_p[2] = {-1, -1};

    // ignore SIGPIPE locally
    struct sigaction sa, old_sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (::sigaction(SIGPIPE, &sa, &old_sa) != 0)
    {
        err = std::string("sigaction: ") + std::strerror(errno);
        return false;
    }
    if (::pipe(in_p) != 0 || ::pipe(out_p) != 0) {
        err = std::string("pipe: ") + std::strerror(errno);
        safe_close(in_p[0]);  safe_close(in_p[1]);
        safe_close(out_p[0]); safe_close(out_p[1]);
        ::sigaction(SIGPIPE, &old_sa, NULL);
        return false;
    }

    envp.reserve(env.size() + 1);
    for (size_t i = 0; i < env.size(); ++i)
        envp.push_back(const_cast<char*>(env[i].c_str()));
    envp.push_back(NULL);

    pid_t pid = ::fork();
    if (pid < 0) {
        err = std::string("fork: ") + std::strerror(errno);
        safe_close(in_p[0]);  safe_close(in_p[1]);
        safe_close(out_p[0]); safe_close(out_p[1]);
        ::sigaction(SIGPIPE, &old_sa, NULL);
        return false;
    }
    if (pid == 0)
    {
        if (::dup2(in_p[0], STDIN_FILENO) < 0 || ::dup2(out_p[1], STDOUT_FILENO) < 0)
            ::_exit(126);
        safe_close(in_p[0]);  safe_close(out_p[0]);
        safe_close(out_p[1]); safe_close(in_p[1]);

        if (!req.working_directory.empty())
        {
            if (::chdir(req.working_directory.c_str()) != 0)
                ::_exit(126);
        }
        if (::sigaction(SIGPIPE, &old_sa, NULL) != 0)
            ::_exit(127);
        char* argv[3];
        if (req.interpreter_path.empty())
        {
            argv[0] = const_cast<char*>(req.script_path.c_str());
            argv[1] = NULL;
            ::execve(argv[0], argv, &envp[0]);
        }
        else
        {
            argv[0] = const_cast<char*>(req.interpreter_path.c_str());
            argv[1] = const_cast<char*>(req.script_path.c_str());
            argv[2] = NULL;
            ::execve(argv[0], argv, &envp[0]);
        }
        ::_exit(127);
    }

    safe_close(in_p[0]);
    safe_close(out_p[1]);

    proc.pid   = pid;
    proc.in_fd = in_p[1];
    proc.out_fd = out_p[0];

    ::sigaction(SIGPIPE, &old_sa, NULL);
    return true;
}

bool CgiHandler::runProcess(const CgiRequest& req, const std::vector<std::string>& env, std::string& raw, std::string& err, int timeout_sec) const
{
    (void)env;
    (void)timeout_sec;
    CgiProcess proc;
    if (!spawn(req, proc, err))
        return false;

    size_t sent = 0;
    char   buf[4096];

    while (sent < req.body.size())
    {
        size_t  chunk = req.body.size() - sent;
        if (chunk > 4096) chunk = 4096;
        ssize_t w = ::write(proc.in_fd, req.body.c_str() + sent, chunk);
        if (w <= 0)
            break;
        sent += (size_t)w;
    }
    safe_close(proc.in_fd);

    ssize_t r;
    while ((r = ::read(proc.out_fd, buf, sizeof(buf))) > 0)
        raw.append(buf, (size_t)r);
    safe_close(proc.out_fd);

    int child_status = 0;
    ::waitpid(proc.pid, &child_status, 0);
    if (WIFSIGNALED(child_status))
    {
        err = "CGI killed by signal";
        return false;
    }
    if (WIFEXITED(child_status) && WEXITSTATUS(child_status) != 0 && raw.empty())
    {
        err = "CGI exited non-zero, no output";
        return false;
    }
    return true;
}

bool CgiHandler::parseOutput(const std::string& raw, CgiResponse& res, std::string& err) const
{
    if (raw.empty()) 
	{
		err = "CGI returned empty output"; 
		return false; 
	}
    size_t sep     = raw.find("\r\n\r\n");
    size_t sep_len = 4;
    if (sep == std::string::npos) 
	{
        sep     = raw.find("\n\n");
        sep_len = 2;
    }
    if (sep == std::string::npos)
	{
        res.body = raw;
        return true;
    }
    std::string headers_raw = raw.substr(0, sep);
    res.body                = raw.substr(sep + sep_len);
    std::istringstream stream(headers_raw);
    std::string        line;
    while (std::getline(stream, line))
	{
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        line = str_trim(line);
        if (line.empty()) continue;
        if (line.compare(0, 5, "HTTP/") == 0)
		{
            std::istringstream ls(line);
            std::string ver;
            ls >> ver >> res.status_code;
            continue;
        }
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = str_trim(line.substr(0, colon));
        std::string val = str_trim(line.substr(colon + 1));
        if (key.empty()) continue;
        if (str_lower(key) == "status")
        {
            std::istringstream vs(val);
            vs >> res.status_code;
            continue;
        }
        res.headers.push_back(std::make_pair(key, val));
    }
    return true;
}