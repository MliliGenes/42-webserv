#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>   // strtoul

// ─────────────────────────────────────────────────────────────────────────────
// Public entry points
// ─────────────────────────────────────────────────────────────────────────────

std::vector<ServerConfig> ConfigParser::parseFile(const std::string& filepath)
{
    std::ifstream f(filepath.c_str());
    if (!f.is_open())
        throw std::runtime_error("ConfigParser: cannot open file: " + filepath);
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseString(ss.str());
}

std::vector<ServerConfig> ConfigParser::parseString(const std::string& text)
{
    TokenList tokens = tokenise(text);
    return parse(tokens);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tokeniser
//
// Produces a flat list of:
//   WORD       – any unquoted run of non-special characters
//   SEMICOLON  – ;
//   OPEN_BRACE – {
//   CLOSE_BRACE– }
//   END        – sentinel at the end
//
// Comments: from '#' to end of line, discarded.
// ─────────────────────────────────────────────────────────────────────────────
ConfigParser::TokenList ConfigParser::tokenise(const std::string& text)
{
    TokenList tokens;
    std::size_t i   = 0;
    int         line = 1;
    std::size_t n   = text.size();

    while (i < n)
    {
        char c = text[i];

        // newline
        if (c == '\n') { ++line; ++i; continue; }

        // whitespace
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

        // comment
        if (c == '#')
        {
            while (i < n && text[i] != '\n') ++i;
            continue;
        }

        // single-character tokens
        if (c == ';') { tokens.push_back(Token(Token::SEMICOLON,    ";", line)); ++i; continue; }
        if (c == '{') { tokens.push_back(Token(Token::OPEN_BRACE,   "{", line)); ++i; continue; }
        if (c == '}') { tokens.push_back(Token(Token::CLOSE_BRACE,  "}", line)); ++i; continue; }

        // quoted string  "…"
        if (c == '"')
        {
            ++i;
            std::string word;
            while (i < n && text[i] != '"')
            {
                if (text[i] == '\\' && i + 1 < n) { ++i; }
                word += text[i++];
            }
            if (i < n) ++i; // closing "
            tokens.push_back(Token(Token::WORD, word, line));
            continue;
        }

        // word: everything else until whitespace or special char
        {
            std::string word;
            while (i < n)
            {
                char wc = text[i];
                if (std::isspace(static_cast<unsigned char>(wc))
                    || wc == ';' || wc == '{' || wc == '}' || wc == '#')
                    break;
                word += wc;
                ++i;
            }
            if (!word.empty())
                tokens.push_back(Token(Token::WORD, word, line));
        }
    }

    tokens.push_back(Token(Token::END, "", line));
    return tokens;
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level parser
// ─────────────────────────────────────────────────────────────────────────────
std::vector<ServerConfig> ConfigParser::parse(const TokenList& tokens)
{
    std::vector<ServerConfig> servers;
    TokIt it  = tokens.begin();
    TokIt end = tokens.end();

    while (peek(it, end).kind != Token::END)
    {
        const Token& t = peek(it, end);
        if (t.kind != Token::WORD || t.value != "server")
            throw std::runtime_error(errAt(t.line,
                "expected 'server' block, got '" + t.value + "'"));
        consume(it, end);  // eat 'server'
        expect(it, end, Token::OPEN_BRACE, "server");
        servers.push_back(parseServer(it, end));
    }

    if (servers.empty())
        throw std::runtime_error("ConfigParser: no server blocks found");

    return servers;
}

// ─────────────────────────────────────────────────────────────────────────────
// parseServer  –  everything inside server { … }
// ─────────────────────────────────────────────────────────────────────────────
ServerConfig ConfigParser::parseServer(TokIt& it, const TokIt& end)
{
    ServerConfig cfg;

    while (peek(it, end).kind != Token::CLOSE_BRACE)
    {
        if (peek(it, end).kind == Token::END)
            throw std::runtime_error("ConfigParser: unexpected end of file inside server block");

        const Token& dir = peek(it, end);
        if (dir.kind != Token::WORD)
            throw std::runtime_error(errAt(dir.line, "expected directive, got '" + dir.value + "'"));

        consume(it, end);  // eat directive name
        std::string name = dir.value;
        int         dline = dir.line;

        // ── location /path { … } ─────────────────────────────────────────────
        if (name == "location")
        {
            const Token& pathTok = expect(it, end, Token::WORD, "location path");
            std::string  locPath = pathTok.value;
            expect(it, end, Token::OPEN_BRACE, "location");
            cfg.routes.push_back(parseLocation(it, end, locPath));
            continue;  // parseLocation already consumed the closing '}'
        }

        // ── collect values before ';' ─────────────────────────────────────────
        std::vector<std::string> vals;
        while (peek(it, end).kind == Token::WORD)
        {
            vals.push_back(peek(it, end).value);
            consume(it, end);
        }
        expect(it, end, Token::SEMICOLON, name.c_str());

        if (vals.empty())
            throw std::runtime_error(errAt(dline, "directive '" + name + "' has no value"));

        // ── dispatch on directive name ────────────────────────────────────────
        if (name == "host")
        {
            cfg.host = vals[0];
        }
        else if (name == "port")
        {
            char* ep = NULL;
            long p = std::strtol(vals[0].c_str(), &ep, 10);
            if (!ep || *ep != '\0' || p <= 0 || p > 65535)
                throw std::runtime_error(errAt(dline, "invalid port: " + vals[0]));
            cfg.port = static_cast<int>(p);
        }
        else if (name == "server_name")
        {
            for (std::size_t i = 0; i < vals.size(); ++i)
                cfg.serverNames.push_back(vals[i]);
        }
        else if (name == "root")
        {
            cfg.root = vals[0];
        }
        else if (name == "index")
        {
            for (std::size_t i = 0; i < vals.size(); ++i)
                cfg.index.push_back(vals[i]);
        }
        else if (name == "max_body" || name == "client_max_body_size")
        {
            cfg.clientMaxBodySize = parseSize(vals[0], dline);
        }
        else if (name == "error_page")
        {
            // error_page <code> <path>;
            if (vals.size() < 2)
                throw std::runtime_error(errAt(dline, "error_page requires code and path"));
            char* ep = NULL;
            long code = std::strtol(vals[0].c_str(), &ep, 10);
            if (!ep || *ep != '\0' || code < 100 || code > 599)
                throw std::runtime_error(errAt(dline, "invalid error code: " + vals[0]));
            cfg.errorPages[static_cast<int>(code)] = vals[1];
        }
        else
        {
            // Unknown directive: warn but don't crash
            // (allows forward-compatibility with directives we don't handle)
            // Silently skip — values and semicolon are already consumed above.
        }
    }

    consume(it, end);  // eat '}'
    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// parseLocation  –  everything inside location /path { … }
// ─────────────────────────────────────────────────────────────────────────────
LocationConfig ConfigParser::parseLocation(TokIt& it, const TokIt& end,
                                           const std::string& path)
{
    LocationConfig loc;
    loc.path = path;

    while (peek(it, end).kind != Token::CLOSE_BRACE)
    {
        if (peek(it, end).kind == Token::END)
            throw std::runtime_error(
                "ConfigParser: unexpected end of file inside location '" + path + "'");

        const Token& dir = peek(it, end);
        if (dir.kind != Token::WORD)
            throw std::runtime_error(errAt(dir.line,
                "expected directive inside location, got '" + dir.value + "'"));

        consume(it, end);
        std::string name  = dir.value;
        int         dline = dir.line;

        // collect values
        std::vector<std::string> vals;
        while (peek(it, end).kind == Token::WORD)
        {
            vals.push_back(peek(it, end).value);
            consume(it, end);
        }
        expect(it, end, Token::SEMICOLON, name.c_str());

        if (vals.empty())
            throw std::runtime_error(errAt(dline,
                "directive '" + name + "' has no value"));

        if (name == "methods")
        {
            for (std::size_t i = 0; i < vals.size(); ++i)
                loc.methods.insert(vals[i]);
        }
        else if (name == "root")
        {
            loc.root = vals[0];
        }
        else if (name == "autoindex")
        {
            loc.autoindex = (vals[0] == "on" || vals[0] == "true" || vals[0] == "1");
        }
        else if (name == "index")
        {
            for (std::size_t i = 0; i < vals.size(); ++i)
                loc.index.push_back(vals[i]);
        }
        else if (name == "upload_enable")
        {
            loc.uploadEnable = (vals[0] == "on" || vals[0] == "true" || vals[0] == "1");
        }
        else if (name == "upload_store")
        {
            loc.uploadStore = vals[0];
        }
        else if (name == "redirect")
        {
            // redirect <code> <url>;
            if (vals.size() < 2)
                throw std::runtime_error(errAt(dline, "redirect requires code and url"));
            char* ep = NULL;
            long code = std::strtol(vals[0].c_str(), &ep, 10);
            if (!ep || *ep != '\0' || code < 100 || code > 599)
                throw std::runtime_error(errAt(dline,
                    "invalid redirect code: " + vals[0]));
            loc.redirect.code    = static_cast<int>(code);
            loc.redirect.url     = vals[1];
            loc.redirect.enabled = true;
        }
        else if (name == "cgi")
        {
            // cgi <.ext> <interpreter>;
            if (vals.size() < 2)
                throw std::runtime_error(errAt(dline, "cgi requires extension and interpreter"));
            loc.cgi[vals[0]] = vals[1];
        }
        else if (name == "max_body" || name == "client_max_body_size")
        {
            // allowed inside location too, but we don't store it in LocationConfig
            // (it lives in ServerConfig); silently ignore to avoid parse errors
            (void)dline;
        }
        // unknown directives are silently skipped
    }

    consume(it, end);  // eat '}'
    return loc;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

const ConfigParser::Token& ConfigParser::peek(TokIt it, const TokIt& end)
{
    if (it == end)
        throw std::runtime_error("ConfigParser: unexpected end of token stream");
    return *it;
}

void ConfigParser::consume(TokIt& it, const TokIt& end)
{
    if (it != end) ++it;
}

const ConfigParser::Token& ConfigParser::expect(TokIt& it, const TokIt& end,
                                                Token::Kind kind, const char* ctx)
{
    const Token& t = peek(it, end);
    if (t.kind != kind)
    {
        std::string expected;
        switch (kind)
        {
            case Token::WORD:        expected = "word";  break;
            case Token::SEMICOLON:   expected = "';'";   break;
            case Token::OPEN_BRACE:  expected = "'{'";   break;
            case Token::CLOSE_BRACE: expected = "'}'";   break;
            default:                 expected = "token"; break;
        }
        throw std::runtime_error(errAt(t.line,
            std::string("in '") + ctx + "': expected " + expected
            + ", got '" + t.value + "'"));
    }
    consume(it, end);
    return t;   // return the token we just consumed (copy is fine, used for value)
}

// parseSize: supports plain bytes or K / M / G suffix
std::size_t ConfigParser::parseSize(const std::string& val, int line)
{
    if (val.empty())
        throw std::runtime_error(errAt(line, "empty size value"));

    char*         ep  = NULL;
    unsigned long num = std::strtoul(val.c_str(), &ep, 10);

    if (ep == val.c_str())
        throw std::runtime_error(errAt(line, "invalid size: " + val));

    std::size_t mult = 1;
    if (*ep == 'K' || *ep == 'k') mult = 1024UL;
    else if (*ep == 'M' || *ep == 'm') mult = 1024UL * 1024UL;
    else if (*ep == 'G' || *ep == 'g') mult = 1024UL * 1024UL * 1024UL;
    else if (*ep != '\0')
        throw std::runtime_error(errAt(line, "invalid size suffix in: " + val));

    return static_cast<std::size_t>(num) * mult;
}

std::string ConfigParser::errAt(int line, const std::string& msg)
{
    std::ostringstream oss;
    oss << "ConfigParser line " << line << ": " << msg;
    return oss.str();
}
