#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>
#include <sstream>


struct Response {
    int                                status_code;
    std::string                        status_message;
    std::map<std::string, std::string> headers;
    std::string                        body;

    Response() : status_code(200), status_message("OK") {}

    std::string build() const {
        std::ostringstream oss;

        oss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";

        std::map<std::string, std::string>::const_iterator it;
        for (it = headers.begin(); it != headers.end(); ++it)
            oss << it->first << ": " << it->second << "\r\n";

        oss << "\r\n";

        oss << body;

        return oss.str();
    }
    std::string build_withoutBody() const {
        std::ostringstream oss;

        oss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";

        std::map<std::string, std::string>::const_iterator it;
        for (it = headers.begin(); it != headers.end(); ++it)
            oss << it->first << ": " << it->second << "\r\n";

        oss << "\r\n";
        return oss.str();
    }
};

#endif
