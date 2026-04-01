#include "ResponseBuilder.hpp"

ResponseBuilder::ResponseBuilder(SessionManager& session) : sessions_(session){

}

Response ResponseBuilder::dispatch(const Request& req, const ServerConfig& config)
{

    const LocationConfig* route = matchRoute(req.path, config);
    if(!route)
        return buildError(404, config);
    if(route->redirect.enabled)
    {
        Response res;
        res.status_code = route->redirect.code;
        res.status_message = statusMessage(route->redirect.code);
        res.headers["Location"] = route->redirect.url;
        res.headers["Content-Length"] = "0";
        applySessionCookie(req, res);
        return res;
    }
    if(!route->methods.empty() && route->methods.find(req.method) == route->methods.end())
        return buildError(405, config);
    
    Response res;
    if (req.method == "GET") return res = handleGet(req, *route, config);
    else if (req.method == "POST") return res = handlePost(req, *route, config);
    else if (req.method == "DELETE") return res = handleDelete(req, *route, config);
    else return res = buildError(405, config);

    applySessionCookie(req, res);
}


void ResponseBuilder::applySessionCookie(const Request& req, Response& res)
{
    std::map<std::string, std::string>::const_iterator it;
    it = req.headers.find("cookie");
    if (it != req.headers.end()){
        std::string sID = SessionManager::Extract_ID(it->second);
        if(!sID.empty() && sessions_.get(sID) != NULL){
            return ;
        }
    }
    std::string sID = sessions_.create();
    res.headers["Set-Cookie"] = SessionManager::buildCookieHeader(sID);
}

const LocationConfig* ResponseBuilder::matchRoute(const std::string& path, const ServerConfig& config) const
{
    const LocationConfig* best = NULL;
    std::size_t best_len = 0;

    for (std::size_t i = 0; i < config.routes.size(); i++)
    {
        const LocationConfig& loc = config.routes[i];
        const std::string prefix = loc.path;

        if(path.substr(0, prefix.size()) == prefix){
            if (prefix.size() > best_len){
                best_len = prefix.size();
                best = &loc;
            }
        }
    }
    return best;
}

Response ResponseBuilder::buildError(int code, const ServerConfig& config)
{
     std::string body;
     
     std::map<int, std::string>::const_iterator it = config.errorPages.find(code);
     if (it != config.errorPages.end() && fileExists(it->second))
        body = readFile(it->second);
    else{
        std::ostringstream oss;
        oss << "<html><head><title>" 
        << code << " " << statusMessage(code) 
        << "<title></head><body><h1>" 
        << code << " " << statusMessage(code) 
        <<"</h1></body></html>";

        body = oss.str();
    }

    Response res;

    res.status_code = code;
    res.status_message = statusMessage(code);
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = sizeToString(body.size());
    res.body = body;
    return res;
}

Response ResponseBuilder::handleGet(const Request&       req, const LocationConfig& route,
                                    const ServerConfig&   config)
{
    std::string root = route.root.empty() ? config.root : route.root;
    std::string fs_path = resolve_path(root, req.path);


    if(!route.cgi.empty())
    {
        std::size_t dot = req.path.rfind('.');
        if (dot != std::string::npos)
        {
            std::string ext = req.path.substr(dot);
            std::map<std::string, std::string>::const_iterator it;
            it = route.cgi.find(ext);
            if (it != route.cgi.end())
            {
                // std::string cgioutput = handleCgi(req, fs_path, it->second);//abdenour's cgi handler it can be a method in an obj inside my obj 
                // return buildeResfromOutput(cgioutput, config);
            }
        }
    }

    if (isDirectory(fs_path))
    {
        for(std::size_t i = 0; i < route.index.size(); i++){
            std::string idx = fs_path;
            if (fs_path[fs_path.size() - 1] != '/') fs_path += '/';
            idx += route.index[i];
            if (fileExists(idx))
            {
                fs_path = idx;
                goto handle_file;
            }
        }

        for(std::size_t i = 0; i < config.index.size(); i++){
            std::string idx = fs_path;
            if (idx[idx.size() - 1] != '/') idx += '/';
            idx += config.index[i];
            if (fileExists(idx))
            {
                fs_path = idx;
                goto handle_file;
            }
        }
        if (route.autoindex)
            return listsDirectory(fs_path, req.path);
        return buildError(403, config);
    }

    handle_file:
    if (fileExists(fs_path))
    {
        Response res;
        res.status_code = 200;
        res.status_message = "ok";
        res.body = readFile(fs_path);
        res.headers["Content-Length"] = sizeToString(res.body.size());
        res.headers["Content-Type"] = getType(fs_path);
        return res;

    }
    return buildError(404, config);
}

Response ResponseBuilder::handlePost(const Request&      req, const LocationConfig& route,
                                    const ServerConfig&  config)
{
    
    std::string root = route.root.empty() ? config.root : route.root;
    std::string fs_path = resolve_path(root, req.path);

    if (!route.cgi.empty())
    {
        std::size_t dot = req.path.rfind('.');
        if (dot != std::string::npos){
            std::string ext = req.path.substr(dot);
            std::map<std::string, std::string>::const_iterator it;
            it = route.cgi.find(ext);
            if (it != route.cgi.end()){
                // std::string cgioutput = handleCgi(req, fs_path, it->second);//abdenour's cgi handler it can be a method in an obj inside my obj 
               // return buildeResfromOutput(cgioutput, config);
            }       
        }
    }
    if (!route.uploadEnable)
        return buildError(403, config);
    if (route.uploadStore.empty())
        return buildError(500, config);
    std::string filename = "upload"; // default name
    std::map<std::string, std::string>::const_iterator it;
    it = req.headers.find("content-disposition");
    if (it != req.headers.end()){
        std::size_t fn = it->second.find("filename=\"");
        if (fn != std::string::npos)
            fn += 10;
        std::size_t fn_end = it->second.find("\"", fn);
        if (fn_end != std::string::npos)
            filename = it->second.substr(fn, fn_end - fn);
    }
    if (filename == "upload") {
        std::ostringstream oss;
        oss << "upload_" << std::time(NULL);
        filename = oss.str();
    }
    
    std::string path = resolve_path(route.uploadStore, filename);
    if (!writeFile(path, req.body))
        return buildError(500, config);
    Response res;

    res.body = "<html><body>uploaded: " + filename + "</body></html>";
    res.status_code = 201;
    res.status_message = statusMessage(201);
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = sizeToString(res.body.size());
    res.headers["Location"] = "/" + filename;

    return res;
}

Response ResponseBuilder::handleDelete(const Request&      req, const LocationConfig& route,
                          const ServerConfig&  config)
{
    std::string root = route.root.empty() ? config.root : route.root;
    std::string fs_path = resolve_path(root, req.path);

    if (!fileExists(fs_path))
        return buildError(404, config);
    if (deleteFile(fs_path))
        return buildError(500, config);
    
    Response res;
    res.status_code = 204;
    res.status_message = statusMessage(204);
    return res;
}

bool ResponseBuilder::deleteFile(std::string path)
{
    return std::remove(path.c_str());
}

bool ResponseBuilder::writeFile(std::string path, std::string content)
{
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(content.c_str(), content.size());
    return file.good();
}

bool ResponseBuilder::isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

std::string ResponseBuilder::resolve_path(std::string root, std::string path)
{
    std::string result = root;
    std::cout << result << std::endl;
    if (!result.empty() && result[result.size() - 1] == '/' && path[0] == '/')
        result.erase(result.size() - 1);
    else if (path[0] != '/' && result[result.size() - 1] != '/')
        result += "/";
    result += path;
    return result;
}

Response ResponseBuilder::buildeResfromOutput(std::string raw, const ServerConfig& config)
{
    if (raw.empty())
        return buildError(500, config);
    
    std::string raw_headers;
    std::string body;

    std::size_t pos = raw.find("\r\n\r\n");
    if (pos != std::string::npos)
    {
        raw_headers = raw.substr(0, pos);
        body = raw.substr(pos + 4);
    }
    else if(pos = raw.find("\n\n") != std::string::npos)
    {
        raw_headers = raw.substr(0, pos);
        body = raw.substr(pos + 2);
    }
    else
        body = raw;
    
    Response res;
    res.body = body;
    res.status_code = 200;
    res.status_message = statusMessage(200);

    std::istringstream iss(raw_headers);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        else if (line.empty())
            break;
        
        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        
        std::string name = RequestParser::ft_trim(RequestParser::ft_toLower(line.substr(0, colon)));
        std::string value = RequestParser::ft_trim(line.substr(0, colon));

        if (name == "status")
        {
            std::istringstream iss(value);
            iss >> res.status_code;
            res.status_message = statusMessage(res.status_code);
        } else{
            res.headers[name] = value;
        }
    }

    res.headers["Content-Length"] = sizeToString(body.size());
    return res;
}

Response ResponseBuilder::listsDirectory(const std::string& fs_path, const std::string& req_path)
{
    DIR* dir = opendir(fs_path.c_str());
    if (!dir) {
        Response res;
        res.status_code    = 403;
        res.status_message = statusMessage(403);
        return res;
    }

    std::ostringstream html;
    html << "<html><head><title>Index of " << req_path << "</title></head>"
         << "<body><h1>Index of " << req_path << "</h1><hr><pre>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;
        std::string href = req_path;
        if (!href.empty() && href[href.size() - 1] != '/') href += '/';
        href += name;
        html << "<a href=\"" << href << "\">" << name << "</a>\n";
    }
    closedir(dir);
    html << "</pre><hr></body></html>";

    std::string body = html.str();
    Response res;
    res.status_code               = 200;
    res.status_message            = statusMessage(200);
    res.body                      = body;
    res.headers["Content-Type"]   = "text/html";
    res.headers["Content-Length"] = sizeToString(body.size());
    return res;
}


std::string ResponseBuilder::statusMessage(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 500: return "Internal Server Error";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

bool ResponseBuilder::fileExists(const std::string& path) const {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}


std::string ResponseBuilder::readFile(const std::string& path) const {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) return std::string();
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::string ResponseBuilder::sizeToString(std::size_t n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

std::string ResponseBuilder::getType(const std::string& path) const {
    std::size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = RequestParser::ft_toLower(path.substr(dot + 1));

    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css")                  return "text/css";
    if (ext == "js")                   return "application/javascript";
    if (ext == "json")                 return "application/json";
    if (ext == "txt")                  return "text/plain";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png")                  return "image/png";
    if (ext == "gif")                  return "image/gif";
    if (ext == "ico")                  return "image/x-icon";
    if (ext == "svg")                  return "image/svg+xml";
    if (ext == "pdf")                  return "application/pdf";
    if (ext == "zip")                  return "application/zip";
    if (ext == "mp4")                  return "video/mp4";
    if (ext == "mp3")                  return "audio/mpeg";
    return "application/octet-stream";
}
