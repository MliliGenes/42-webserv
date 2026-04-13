#pragma once
#ifndef TRPJSON_HPP
#define TRPJSON_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <iostream>
// ─── forward declarations ────────────────────────────────────────────────────
struct ITrpJsonValue;
struct TrpJsonNull;
struct TrpJsonBool;
struct TrpJsonNumber;
struct TrpJsonString;
struct TrpJsonArray;
struct TrpJsonObject;

// ─── value kinds ─────────────────────────────────────────────────────────────
enum TrpJsonKind { TRP_NULL, TRP_BOOL, TRP_NUMBER, TRP_STRING, TRP_ARRAY, TRP_OBJECT };

struct ITrpJsonValue {
    virtual ~ITrpJsonValue() {}
    virtual TrpJsonKind kind() const = 0;

    // convenience casts – throw std::runtime_error on wrong type
    TrpJsonObject& asObject();
    TrpJsonArray&  asArray();
    TrpJsonString& asString();
    TrpJsonNumber& asNumber();
    TrpJsonBool&   asBool();

    const TrpJsonObject& asObject() const;
    const TrpJsonArray&  asArray()  const;
    const TrpJsonString& asString() const;
    const TrpJsonNumber& asNumber() const;
    const TrpJsonBool&   asBool()   const;
};

struct TrpJsonNull : ITrpJsonValue {
    TrpJsonKind kind() const { return TRP_NULL; }
};

struct TrpJsonBool : ITrpJsonValue {
    bool value;
    TrpJsonBool(bool v) : value(v) {}
    TrpJsonKind kind() const { return TRP_BOOL; }
};

struct TrpJsonNumber : ITrpJsonValue {
    double value;
    TrpJsonNumber(double v) : value(v) {}
    TrpJsonKind kind() const { return TRP_NUMBER; }
    long   asLong()   const { return (long)value; }
    int    asInt()    const { return (int)value; }
};

struct TrpJsonString : ITrpJsonValue {
    std::string value;
    TrpJsonString(const std::string& v) : value(v) {}
    TrpJsonKind kind() const { return TRP_STRING; }
};

struct TrpJsonArray : ITrpJsonValue {
    std::vector<ITrpJsonValue*> items;
    ~TrpJsonArray() { for (size_t i = 0; i < items.size(); ++i) delete items[i]; }
    TrpJsonKind kind() const { return TRP_ARRAY; }
    size_t size() const { return items.size(); }
    ITrpJsonValue* operator[](size_t i) const { return items[i]; }
};

struct TrpJsonObject : ITrpJsonValue {
    std::map<std::string, ITrpJsonValue*> fields;
    ~TrpJsonObject() {
        for (std::map<std::string, ITrpJsonValue*>::iterator it = fields.begin(); it != fields.end(); ++it)
            delete it->second;
    }
    TrpJsonKind kind() const { return TRP_OBJECT; }

    bool has(const std::string& key) const { return fields.count(key) > 0; }

    ITrpJsonValue* get(const std::string& key) const {
        std::map<std::string, ITrpJsonValue*>::const_iterator it = fields.find(key);
        if (it == fields.end()) return NULL;
        return it->second;
    }

    ITrpJsonValue& operator[](const std::string& key) const {
        ITrpJsonValue* v = get(key);
        if (!v) throw std::runtime_error("JSON key not found: " + key);
        return *v;
    }
};

// ─── cast implementations ─────────────────────────────────────────────────────
inline TrpJsonObject& ITrpJsonValue::asObject() {
    TrpJsonObject* p = dynamic_cast<TrpJsonObject*>(this);
    if (!p) throw std::runtime_error("JSON: expected object");
    return *p;
}
inline TrpJsonArray& ITrpJsonValue::asArray() {
    TrpJsonArray* p = dynamic_cast<TrpJsonArray*>(this);
    if (!p) throw std::runtime_error("JSON: expected array");
    return *p;
}
inline TrpJsonString& ITrpJsonValue::asString() {
    TrpJsonString* p = dynamic_cast<TrpJsonString*>(this);
    if (!p) throw std::runtime_error("JSON: expected string");
    return *p;
}
inline TrpJsonNumber& ITrpJsonValue::asNumber() {
    TrpJsonNumber* p = dynamic_cast<TrpJsonNumber*>(this);
    if (!p) throw std::runtime_error("JSON: expected number");
    return *p;
}
inline TrpJsonBool& ITrpJsonValue::asBool() {
    TrpJsonBool* p = dynamic_cast<TrpJsonBool*>(this);
    if (!p) throw std::runtime_error("JSON: expected bool");
    return *p;
}
inline const TrpJsonObject& ITrpJsonValue::asObject() const { return const_cast<ITrpJsonValue*>(this)->asObject(); }
inline const TrpJsonArray&  ITrpJsonValue::asArray()  const { return const_cast<ITrpJsonValue*>(this)->asArray();  }
inline const TrpJsonString& ITrpJsonValue::asString() const { return const_cast<ITrpJsonValue*>(this)->asString(); }
inline const TrpJsonNumber& ITrpJsonValue::asNumber() const { return const_cast<ITrpJsonValue*>(this)->asNumber(); }
inline const TrpJsonBool&   ITrpJsonValue::asBool()   const { return const_cast<ITrpJsonValue*>(this)->asBool();   }

// ─── parser ──────────────────────────────────────────────────────────────────
class TrpJsonParser {
    std::string src;
    size_t             pos;

    void skipWs() {
        while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos;
    }

    char peek() { skipWs(); return pos < src.size() ? src[pos] : '\0'; }
    char consume() { return src[pos++]; }

    std::string parseStringRaw() {
        if (consume() != '"') throw std::runtime_error("JSON: expected '\"'");
        std::string result;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\') {
                ++pos;
                char c = consume();
                switch (c) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    default:   result += c;    break;
                }
            } else {
                result += consume();
            }
        }
        if (pos >= src.size()) throw std::runtime_error("JSON: unterminated string");
        ++pos; // closing "
        return result;
    }

    ITrpJsonValue* parseValue() {
        char c = peek();
        if (c == '"')  return new TrpJsonString(parseStringRaw());
        if (c == '{')  return parseObject();
        if (c == '[')  return parseArray();
        if (c == 't')  { pos += 4; return new TrpJsonBool(true);  }
        if (c == 'f')  { pos += 5; return new TrpJsonBool(false); }
        if (c == 'n')  { pos += 4; return new TrpJsonNull();       }
        if (c == '-' || std::isdigit((unsigned char)c)) return parseNumber();
        printf("%d\n", c);
        std::cerr << "Unexpected char: '" << c 
          << "' at position " << pos << std::endl;

        std::cerr << "Context: " 
          << src.substr(pos > 10 ? pos - 10 : 0, 20) 
          << std::endl;
        throw std::runtime_error(std::string("JSON: unexpected char '") + c + "'");
    }

    ITrpJsonValue* parseNumber() {
        size_t start = pos;
        if (src[pos] == '-') ++pos;
        while (pos < src.size() && (std::isdigit((unsigned char)src[pos]) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-'))
            ++pos;
        double val = atof(src.c_str() + start);
        return new TrpJsonNumber(val);
    }

    ITrpJsonValue* parseObject() {
        consume(); // {
        TrpJsonObject* obj = new TrpJsonObject();
        skipWs();
        if (peek() == '}') { consume(); return obj; }
        while (true) {
            skipWs();
            std::string key = parseStringRaw();
            skipWs();
            if (consume() != ':') throw std::runtime_error("JSON: expected ':'");
            obj->fields[key] = parseValue();
            skipWs();
            char next = peek();
            if (next == '}') { consume(); break; }
            if (next == ',') { consume(); continue; }
            throw std::runtime_error("JSON: expected ',' or '}'");
        }
        return obj;
    }

    ITrpJsonValue* parseArray() {
        consume(); // [
        TrpJsonArray* arr = new TrpJsonArray();
        skipWs();
        if (peek() == ']') { consume(); return arr; }
        while (true) {
            arr->items.push_back(parseValue());
            skipWs();
            char next = peek();
            if (next == ']') { consume(); break; }
            if (next == ',') { consume(); continue; }
            throw std::runtime_error("JSON: expected ',' or ']'");
        }
        return arr;
    }

public:
    TrpJsonParser(const std::string& s) : src(s), pos(0) {}

    ITrpJsonValue* parse() {
        ITrpJsonValue* v = parseValue();
        skipWs();
        return v;
    }
};

// ─── file loader ─────────────────────────────────────────────────────────────
inline ITrpJsonValue* trpJsonParseFile(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    std::ostringstream oss;
    oss << f.rdbuf();
    TrpJsonParser p(oss.str());
    return p.parse();
}

#endif
