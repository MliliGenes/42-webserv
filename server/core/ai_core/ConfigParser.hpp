#pragma once

#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Configcopy.hpp"
#include <string>
#include <vector>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// ConfigParser
//
// Parses a nginx-style config file into a list of ServerConfig.
//
// Config syntax:
//
//   server {
//       host        0.0.0.0;
//       port        8080;
//       server_name localhost webserv.local;
//       root        ./www;
//       index       index.html index.htm;
//       max_body    10M;              # bytes, K, M, G suffixes accepted
//       error_page  404 ./www/404.html;
//
//       location /path {
//           methods       GET POST;
//           root          ./other;   # overrides server root for this location
//           autoindex     on;
//           index         index.php;
//           upload_enable on;
//           upload_store  ./uploads;
//           redirect      301 /new-path;
//           cgi           .py /usr/bin/python3;
//           cgi           .sh /bin/sh;
//       }
//   }
//
// Rules:
//   - Lines starting with '#' (after whitespace) are comments.
//   - Directives end with ';'.
//   - Multi-value directives (index, server_name, methods) list values
//     space-separated before the ';'.
//   - Blocks are delimited by '{' and '}'.
//   - Multiple server blocks in one file are supported.
// ─────────────────────────────────────────────────────────────────────────────

class ConfigParser
{
public:
    // Parse a file on disk.  Throws std::runtime_error on any error.
    static std::vector<ServerConfig> parseFile(const std::string& filepath);

    // Parse a string directly (useful for unit tests).
    static std::vector<ServerConfig> parseString(const std::string& text);

private:
    // ── tokeniser ────────────────────────────────────────────────────────────
    struct Token {
        enum Kind { WORD, SEMICOLON, OPEN_BRACE, CLOSE_BRACE, END };
        Kind        kind;
        std::string value;
        int         line;

        Token(Kind k, const std::string& v, int l)
            : kind(k), value(v), line(l) {}
    };

    typedef std::vector<Token>                  TokenList;
    typedef TokenList::const_iterator           TokIt;

    static TokenList tokenise(const std::string& text);

    // ── recursive-descent parser ─────────────────────────────────────────────
    static std::vector<ServerConfig> parse(const TokenList& tokens);
    static ServerConfig   parseServer  (TokIt& it, const TokIt& end);
    static LocationConfig parseLocation(TokIt& it, const TokIt& end,
                                        const std::string& path);

    // ── helpers ───────────────────────────────────────────────────────────────
    static const Token& expect(TokIt& it, const TokIt& end,
                               Token::Kind kind, const char* ctx);
    static const Token& peek   (TokIt it,  const TokIt& end);
    static void         consume(TokIt& it, const TokIt& end);

    static std::size_t parseSize(const std::string& val, int line);
    static std::string errAt(int line, const std::string& msg);
};

#endif
