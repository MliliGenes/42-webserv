#include "Config.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>

// ─── helpers ─────────────────────────────────────────────────────────────────

static size_t parseSize(const std::string& s) {
    if (s.empty()) return 0;
    char   last  = s[s.size() - 1];
    size_t value = (size_t)std::atol(s.c_str());
    if (last == 'M' || last == 'm') return value * 1024 * 1024;
    if (last == 'K' || last == 'k') return value * 1024;
    if (last == 'G' || last == 'g') return value * 1024 * 1024 * 1024;
    return value;
}

static std::string getString(const TrpJsonObject& obj, const std::string& key,
                              const std::string& def = "") {
    ITrpJsonValue* v = obj.get(key);
    if (!v) return def;
    if (v->kind() == TRP_STRING) return v->asString().value;
    if (v->kind() == TRP_NUMBER) {
        std::ostringstream oss;
        oss << (long)v->asNumber().value;
        return oss.str();
    }
    return def;
}

static int getInt(const TrpJsonObject& obj, const std::string& key, int def = 0) {
    ITrpJsonValue* v = obj.get(key);
    if (!v) return def;
    if (v->kind() == TRP_NUMBER) return v->asNumber().asInt();
    if (v->kind() == TRP_STRING) return std::atoi(v->asString().value.c_str());
    return def;
}

static bool getBool(const TrpJsonObject& obj, const std::string& key, bool def = false) {
    ITrpJsonValue* v = obj.get(key);
    if (!v) return def;
    if (v->kind() == TRP_BOOL)   return v->asBool().value;
    if (v->kind() == TRP_STRING) return v->asString().value == "true" || v->asString().value == "on";
    return def;
}

// ─── location parser ──────────────────────────────────────────────────────────

static LocationConfig parseLocation(const TrpJsonObject& obj) {
    LocationConfig loc;

    loc.path        = getString(obj, "path", "/");
    loc.root        = getString(obj, "root");
    loc.autoindex   = getBool(obj, "autoindex", false);
    loc.uploadEnable = getBool(obj, "upload_enable", false);
    loc.uploadStore  = getString(obj, "upload_store");

    // methods array
    ITrpJsonValue* methods = obj.get("methods");
    if (methods && methods->kind() == TRP_ARRAY) {
        TrpJsonArray& arr = methods->asArray();
        for (size_t i = 0; i < arr.size(); ++i)
            if (arr[i]->kind() == TRP_STRING)
                loc.methods.insert(arr[i]->asString().value);
    }

    // index array
    ITrpJsonValue* idx = obj.get("index");
    if (idx && idx->kind() == TRP_ARRAY) {
        TrpJsonArray& arr = idx->asArray();
        for (size_t i = 0; i < arr.size(); ++i)
            if (arr[i]->kind() == TRP_STRING)
                loc.index.push_back(arr[i]->asString().value);
    } else if (idx && idx->kind() == TRP_STRING) {
        loc.index.push_back(idx->asString().value);
    }

    // redirect object
    ITrpJsonValue* redir = obj.get("redirect");
    if (redir && redir->kind() == TRP_OBJECT) {
        TrpJsonObject& ro = redir->asObject();
        loc.redirect.enabled = true;
        loc.redirect.code    = getInt(ro, "code", 301);
        loc.redirect.url     = getString(ro, "url");
    }

    // cgi map  { ".py": "/usr/bin/python3", ... }
    ITrpJsonValue* cgi = obj.get("cgi");
    if (cgi && cgi->kind() == TRP_OBJECT) {
        TrpJsonObject& co = cgi->asObject();
        for (std::map<std::string, ITrpJsonValue*>::const_iterator it = co.fields.begin();
             it != co.fields.end(); ++it) {
            if (it->second->kind() == TRP_STRING)
                loc.cgi[it->first] = it->second->asString().value;
        }
    }

    return loc;
}

// ─── server parser ────────────────────────────────────────────────────────────

static ServerConfig parseServer(const TrpJsonObject& obj) {
    ServerConfig srv;

    srv.host = getString(obj, "host", "0.0.0.0");
    srv.port = getInt(obj, "port", 80);
    srv.root = getString(obj, "root", "./www");

    // client_max_body_size  (string like "10M" or number)
    ITrpJsonValue* maxBody = obj.get("client_max_body_size");
    if (maxBody) {
        if (maxBody->kind() == TRP_STRING)
            srv.clientMaxBodySize = parseSize(maxBody->asString().value);
        else if (maxBody->kind() == TRP_NUMBER)
            srv.clientMaxBodySize = (size_t)maxBody->asNumber().value;
    }

    // server_name  (string or array)
    ITrpJsonValue* sn = obj.get("server_name");
    if (sn) {
        if (sn->kind() == TRP_ARRAY) {
            TrpJsonArray& arr = sn->asArray();
            for (size_t i = 0; i < arr.size(); ++i)
                if (arr[i]->kind() == TRP_STRING)
                    srv.serverNames.push_back(arr[i]->asString().value);
        } else if (sn->kind() == TRP_STRING) {
            srv.serverNames.push_back(sn->asString().value);
        }
    }

    // index array
    ITrpJsonValue* idx = obj.get("index");
    if (idx && idx->kind() == TRP_ARRAY) {
        TrpJsonArray& arr = idx->asArray();
        for (size_t i = 0; i < arr.size(); ++i)
            if (arr[i]->kind() == TRP_STRING)
                srv.index.push_back(arr[i]->asString().value);
    } else if (idx && idx->kind() == TRP_STRING) {
        srv.index.push_back(idx->asString().value);
    }

    // error_pages object  { "404": "./www/404.html", ... }
    ITrpJsonValue* ep = obj.get("error_pages");
    if (ep && ep->kind() == TRP_OBJECT) {
        TrpJsonObject& eo = ep->asObject();
        for (std::map<std::string, ITrpJsonValue*>::const_iterator it = eo.fields.begin();
             it != eo.fields.end(); ++it) {
            int code = std::atoi(it->first.c_str());
            if (code > 0 && it->second->kind() == TRP_STRING)
                srv.errorPages[code] = it->second->asString().value;
        }
    }

    // locations array
    ITrpJsonValue* locs = obj.get("locations");
    if (locs && locs->kind() == TRP_ARRAY) {
        TrpJsonArray& arr = locs->asArray();
        for (size_t i = 0; i < arr.size(); ++i)
            if (arr[i]->kind() == TRP_OBJECT)
                srv.routes.push_back(parseLocation(arr[i]->asObject()));
    }

    return srv;
}

// ─── Config constructor ───────────────────────────────────────────────────────

Config::Config(ITrpJsonValue* ast) {
    if (!ast) throw std::runtime_error("Config: null AST");

    // top-level must be an array  [ { "server": {...} }, ... ]
    if (ast->kind() != TRP_ARRAY)
        throw std::runtime_error("Config: top level must be a JSON array");

    TrpJsonArray& root = ast->asArray();
    for (size_t i = 0; i < root.size(); ++i) {
        if (root[i]->kind() != TRP_OBJECT) continue;
        TrpJsonObject& entry = root[i]->asObject();

        ITrpJsonValue* srv = entry.get("server");
        if (!srv || srv->kind() != TRP_OBJECT) continue;

        _servers.push_back(parseServer(srv->asObject()));
    }

    if (_servers.empty())
        throw std::runtime_error("Config: no valid server blocks found");
}

// ─── prettyPrint ─────────────────────────────────────────────────────────────

void Config::prettyPrint() const {
    std::cout << _servers.size() << " valid server blocks loaded into configuration.\n";
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerConfig& s = _servers[i];
        std::cout << "Server [" << i << "]:\n";
        std::cout << "  host:              " << s.host << "\n";
        std::cout << "  port:              " << s.port << "\n";
        std::cout << "  clientMaxBodySize: " << s.clientMaxBodySize << "\n";
        std::cout << "  root:              " << s.root << "\n";
        std::cout << "  serverNames:       ";
        for (size_t j = 0; j < s.serverNames.size(); ++j)
            std::cout << s.serverNames[j] << (j + 1 < s.serverNames.size() ? ", " : "");
        std::cout << "\n";
        std::cout << "  index:             ";
        for (size_t j = 0; j < s.index.size(); ++j)
            std::cout << s.index[j] << (j + 1 < s.index.size() ? ", " : "");
        std::cout << "\n";
        std::cout << "  errorPages:\n";
        for (std::map<int,std::string>::const_iterator it = s.errorPages.begin(); it != s.errorPages.end(); ++it)
            std::cout << "    " << it->first << " -> " << it->second << "\n";
        for (size_t j = 0; j < s.routes.size(); ++j) {
            const LocationConfig& l = s.routes[j];
            std::cout << "  Location [" << j << "]: " << l.path << "\n";
            std::cout << "    root:         " << l.root << "\n";
            std::cout << "    autoindex:    " << (l.autoindex ? "on" : "off") << "\n";
            std::cout << "    uploadEnable: " << (l.uploadEnable ? "on" : "off") << "\n";
            std::cout << "    uploadStore:  " << l.uploadStore << "\n";
            std::cout << "    methods:      ";
            for (std::set<std::string>::const_iterator it = l.methods.begin(); it != l.methods.end(); ++it)
                std::cout << *it << " ";
            std::cout << "\n";
            if (!l.cgi.empty()) {
                std::cout << "    cgi:\n";
                for (std::map<std::string,std::string>::const_iterator it = l.cgi.begin(); it != l.cgi.end(); ++it)
                    std::cout << "      " << it->first << " -> " << it->second << "\n";
            }
            if (l.redirect.enabled)
                std::cout << "    redirect:     " << l.redirect.code << " -> " << l.redirect.url << "\n";
        }
    }
}
