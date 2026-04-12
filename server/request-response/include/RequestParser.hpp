#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include "Request.hpp"
#include <string>

class RequestParser {

    public:
    enum Status { Incomplete, Complete, Error };

    RequestParser(std::size_t max_body_size = 1024 * 1024);
    ~RequestParser();

    Status feed(const char* data, std::size_t len);

    const Request& getRequest()     const;
    int getErrorCode() const;
    void reset();
    void setMaxBodySize(std::size_t size);
    
    static std::string ft_trim(const std::string& s);
    static std::string ft_toLower(const std::string& s);
    private:
        std::string  buffer_;
        Request      request_;
        bool         request_line_parsed_;
        bool         headers_parsed_;
        bool         chunked_;
        std::size_t  content_length_;
        std::string  chunk_buffer_;
        std::size_t  max_body_size_;
        int          error_code_;

        int parseRequestLine();
        int parseHeaders();
        Status handleBody();
        bool decodeChunked();

        bool isAllowedMethod(const std::string& methode) const;
        bool isAllowedVersion(const std::string& version) const;

};

#endif