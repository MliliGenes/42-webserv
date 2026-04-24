#include "RequestParser.hpp"
#include <sstream>
#include <cstdlib>
#include <cctype>

static const char* allowedMethode[] = {"POST", "GET", "DELETE", NULL};
static const char* allowedVersion[] = {"HTTP/1.1", "HTTP/1.0", NULL};


RequestParser::~RequestParser() {}

RequestParser::RequestParser(std::size_t max_body_size)
    : buffer_(), request_(), request_line_parsed_(false),
      headers_parsed_(false), chunked_(false), content_length_(0),
      chunk_buffer_(), max_body_size_(max_body_size), error_code_(0) {}

const Request& RequestParser::getRequest() const { return request_; }

int RequestParser::getErrorCode() const { return error_code_; }

void RequestParser::reset() {
    buffer_.clear();
    request_          = Request();
    request_line_parsed_ = false;
    headers_parsed_   = false;
    chunked_          = false;
    content_length_   = 0;
    chunk_buffer_.clear();
    error_code_       = 0;
}

void RequestParser::setMaxBodySize(std::size_t size) {
    max_body_size_ = size;
}


RequestParser::Status RequestParser::feed(const char* data, std::size_t len)
{
    if(len)
        buffer_.append(data, len);
        
    if (!request_line_parsed_)
    {
        int r = parseRequestLine();
        if (r == -1) return Error;
        if (r == 0) return Incomplete;
    }

    if (!headers_parsed_)
    {
        int r = parseHeaders();
        if (r == -1) return Error;
        if (r == 0) return Incomplete;
    }
    
    return handleBody();
}

int RequestParser::parseRequestLine()
{
    std::size_t pos = buffer_.find("\r\n");
    if (pos == std::string::npos)
        return 0;
    
    std::string line = buffer_.substr(0, pos);
    buffer_.erase(0, pos + 2);

    std::istringstream iss(line);


    if (!(iss >> request_.method))
    {
        error_code_ = 400;
        return -1;
    }
    if(!isAllowedMethod(request_.method))
    {
        error_code_ = 405;
        return -1;
    }

    std::string target;
    if (!(iss >> target))
    {
        error_code_ = 400;
        return -1;
    }

    if(!(iss >> request_.version))
    {
        error_code_ = 400;
        return -1;
    }
    if (!isAllowedVersion(request_.version))
    {
        error_code_ = 505;
        return -1;
    }

    // get https://adnanlwa3r.com/path?a=1 http/1.1
    if (target.substr(0, 7) == "http://" || target.substr(0, 8) == "https://")
    {
        std::size_t slash = target.find('/', 8);
        if (slash == std::string::npos)
            target = "/"; // by default kykon root( index.html okda)
        else
            target = target.substr(slash); // safi get rid of https://adnanlwa3r.com/
    }

    std::size_t q = target.find('?');
    if (q != std::string::npos)
    {
        request_.path = target.substr(0, q);
        request_.query = target.substr(q + 1);
    }else{
        request_.path = target;
        request_.query.clear();
    }

    request_line_parsed_ = true;

    return 1;
}

int RequestParser::parseHeaders()
{
    std::size_t pos = buffer_.find("\r\n\r\n");
    if (pos == std::string::npos)
        return 0;
    std::string hdrs = buffer_.substr(0, pos + 2);
    buffer_.erase(0, pos + 4);
    
    std::istringstream iss(hdrs);
    std::string line;
    while(std::getline(iss, line))
    {
        if(!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if(line.empty())
            break;
        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            error_code_ = 400;
            return -1;
        }

        std::string name = ft_trim(ft_toLower(line.substr(0, colon)));
        std::string value = ft_trim(line.substr(colon + 1));
        request_.headers[name] = value;
    }

    std::map<std::string, std::string>::const_iterator it;
    it = request_.headers.find("host");
    if(it == request_.headers.end() && request_.version == "HTTP/1.1"){
        error_code_ = 400;
        return -1;
    }

    it = request_.headers.find("transfer-encoding");
    if (it != request_.headers.end())
    {
        if(ft_toLower(it->second).find("chunked") != std::string::npos)
            chunked_ = true;
    }

    it = request_.headers.find("content-length");
    if (it != request_.headers.end() && !chunked_)
    {
        const std::string& val = it->second;
        char* end = NULL;

        unsigned long parsed = std::strtoul(val.c_str(), &end, 10);
        if (val.c_str() == end || *end != '\0')
        {
            error_code_ = 400;
            return -1;
        }
        
        content_length_ = static_cast<std::size_t>(parsed);

        if (max_body_size_ > 0 && max_body_size_ < content_length_)
        {
            error_code_ = 413;
            return -1;
        }
    }

    it = request_.headers.find("connection");
    if (it == request_.headers.end()){
        request_.headers["connection"] = "keep-alive";
    }
    headers_parsed_ = true;
     return 1;

}

RequestParser::Status RequestParser::handleBody()
{
    if (chunked_)
    {
        if (!decodeChunked())
        {
            if (error_code_ != 0)
                return Error;
            return Incomplete;
        }
        request_.body = chunk_buffer_;
        return Complete;
    }
    if (content_length_ > 0)
    {
        if (buffer_.size() < content_length_)
            return Incomplete;
        request_.body = buffer_.substr(0, content_length_);
        buffer_.erase(0, content_length_);
        return Complete;
    }

    request_.body.clear();
    return Complete;
}

bool RequestParser::decodeChunked()
{
    while(1337)
    {
        std::size_t line_end = buffer_.find("\r\n");
        if (line_end == std::string::npos)
            return false;
        std::string chunk_size = buffer_.substr(0, line_end);// ***

        std::size_t semi = chunk_size.find(';');
        if (semi != std::string::npos)
            chunk_size = chunk_size.substr(0, semi);

        char* end = NULL;
        unsigned long len = std::strtoul(chunk_size.c_str(), &end, 16);//***
        if (chunk_size.c_str() == end || *end != '\0')
        {
            error_code_ = 400;
            return false;
        }

        std::size_t chunk_start = line_end + 2;

        if (len == 0)
        {
            if (buffer_.size() >= chunk_start + 2 && buffer_.substr(chunk_start, 2) == "\r\n")
            {
                buffer_.erase(0, chunk_start + 2);
                return true;
            }

            std::size_t last_crlf = buffer_.find("\r\n\r\n", chunk_start);
            if (last_crlf == std::string::npos)
                return false;
            buffer_.erase(0, last_crlf + 4);
            return true;
        }

        if (chunk_start + len + 2 > buffer_.size())
            return false;
        
        if (max_body_size_ > 0 && chunk_buffer_.size() + len > max_body_size_)
        {
            error_code_ = 413;
            return false;
        }
        chunk_buffer_.append(buffer_, chunk_start, len);
        buffer_.erase(0, chunk_start + len + 2);
    }
}


bool RequestParser::isAllowedMethod(const std::string& methode) const
{
    for(int i = 0; allowedMethode[i]; i++){
        if (allowedMethode[i] == methode)
            return true;
    }
    return false;
}

bool RequestParser::isAllowedVersion(const std::string& version) const {
    for (int i = 0; allowedVersion[i] != NULL; ++i) {
        if (version == allowedVersion[i])
            return true;
    }
    return false;
}

std::string RequestParser::ft_trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string RequestParser::ft_toLower(const std::string& s) {
    std::string out = s;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] >= 'A' && out[i] <= 'Z')
            out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}