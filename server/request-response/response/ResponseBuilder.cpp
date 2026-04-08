#include <iostream>
#include <fstream>
#include <cstdio>
#include <cctype>
#include "ResponseBuilder.hpp"
#include "Response.hpp"

static std::string cgiHeaderName(const std::string& name)
{
    std::string result = "HTTP_";
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        char c = name[i];
        if (c == '-')
            result += '_';
        else if (c >= 'a' && c <= 'z')
            result += static_cast<char>(c - 'a' + 'A');
        else
            result += c;
    }
    return result;
}

static void fillCgiRequest(const Request& req, const std::string& script_path,
                           const std::string& working_directory,
                           const std::string& interpreter_path,
                           cgirequest& cgireq)
{
    cgireq.method = req.method;
    cgireq.script_path = script_path;
    cgireq.interpreter_path = interpreter_path;
    cgireq.query_string = req.query;
    cgireq.body = req.body;
    cgireq.working_directory = working_directory;

    std::map<std::string, std::string>::const_iterator it = req.headers.find("content-type");
    if (it != req.headers.end())
        cgireq.content_type = it->second;

    for (it = req.headers.begin(); it != req.headers.end(); ++it)
    {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        cgireq.extra_env[cgiHeaderName(it->first)] = it->second;
    }
}

ResponseBuilder::ResponseBuilder(SessionManager& session) : sessions_(session){}

Response ResponseBuilder::dispatch(const Request& req, const ServerConfig& config, cgihandler& cgi){

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
    if (req.method == "GET")  res = handleGet(req, *route, config, cgi);
    else if (req.method == "POST")  res = handlePost(req, *route, config, cgi);
    else if (req.method == "DELETE")  res = handleDelete(req, *route, config, cgi);
    else return res = buildError(405, config);

    if (res.headers.find("Set-Cookie") == res.headers.end())
        applySessionCookie(req, res);

    return res;
}

void ResponseBuilder::applySessionCookie(const Request& req, Response& res){
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

const LocationConfig* ResponseBuilder::matchRoute(const std::string& path, const ServerConfig& config) const{
    const LocationConfig* best = NULL;
    std::size_t best_len = 0;

    for (std::size_t i = 0; i < config.routes.size(); i++)
    {
        const LocationConfig& loc = config.routes[i];
        const std::string prefix = loc.path;

        if(path == prefix || prefix == "/" || (path.substr(0, prefix.size()) == prefix && 
            (path.size() > prefix.size() && path[prefix.size()] == '/'))){
            if (prefix.size() > best_len){
                best_len = prefix.size();
                best = &loc;
            }
        }
    }
    return best;
}

Response ResponseBuilder::buildError(int code, const ServerConfig& config){
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
                                    const ServerConfig&   config, cgihandler& cgi){
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
				cgirequest	cgireq;
				cgiresponse	cgires;
                std::string error;

                std::string working_directory = root;
                std::size_t slash = fs_path.rfind('/');
                if (slash != std::string::npos)
                    working_directory = fs_path.substr(0, slash);

                fillCgiRequest(req, fs_path, working_directory, it->second, cgireq);

                bool st = cgi.execute(cgireq, cgires, error);
                if (!st)
                {
                    std::cout << error << std::endl;
                    return buildError(500, config);
                }

				Response res;	

				res.body = cgires.body;
                res.status_code = cgires.status_code;
				res.status_message = statusMessage(res.status_code);
                for (std::vector<std::pair<std::string, std::string> >::const_iterator hdr = cgires.headers.begin();
                     hdr != cgires.headers.end(); ++hdr)
                    res.headers[hdr->first] = hdr->second;
                res.headers["Content-Length"] = sizeToString(res.body.size());
                if (res.headers.find("Content-Type") == res.headers.end())
                    res.headers["Content-Type"] = "text/html";

                return res;
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
        res.status_message = statusMessage(200);
        res.body = readFile(fs_path);
        res.headers["Content-Length"] = sizeToString(res.body.size());
        res.headers["Content-Type"] = getType(fs_path);
        return res;

    }
    return buildError(404, config);
}

Response ResponseBuilder::handlePost(const Request&      req, const LocationConfig& route,
                                    const ServerConfig&  config, cgihandler& cgi)
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
				cgirequest	cgireq;
				cgiresponse	cgires;
                std::string error;

                std::string working_directory = root;
                std::size_t slash = fs_path.rfind('/');
                if (slash != std::string::npos)
                    working_directory = fs_path.substr(0, slash);

                fillCgiRequest(req, fs_path, working_directory, it->second, cgireq); // zidt hadi kn 3amar biha data f class d cgi
                bool st = cgi.execute(cgireq, cgires, error);
                if (!st)
                {
                    std::cout << error << std::endl; // error rah string kn amar fiha xmn error w9a3 la st return false
                    return buildError(500, config);
                }
				Response res;
				res.body = cgires.body;
				res.status_code = cgires.status_code;
				res.status_message = statusMessage(res.status_code);
                for (std::vector<std::pair<std::string, std::string> >::const_iterator hdr = cgires.headers.begin();
                     hdr != cgires.headers.end(); ++hdr)
                    res.headers[hdr->first] = hdr->second;
                res.headers["Content-Length"] = sizeToString(res.body.size());
                if (res.headers.find("Content-Type") == res.headers.end())
                    res.headers["Content-Type"] = "text/html";

                return res;
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
                          const ServerConfig&  config, cgihandler& cgi)
{
    (void)cgi;
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
