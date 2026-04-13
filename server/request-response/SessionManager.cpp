#include "include/SessionManager.hpp"
#include <cstdlib>

SessionManager::SessionManager()
{

}

session*    SessionManager::get(const std::string& ID)
{
    std::map<std::string, session>::iterator it;
    it = sessions_.find(ID);
    if (it == sessions_.end())
        return NULL;
    if (std::time(NULL) - it->second.last_access > SESSION_TTL){
        sessions_.erase(it);
        return NULL;
    }
    it->second.last_access = std::time(NULL);
    return &it->second;
}
std::string SessionManager::create()
{
    session se;

    std::ostringstream oss;
    oss << std::time(NULL);
    se.data["created_at"] = oss.str();
    se.id = genarate_ID();
    sessions_[se.id] = se;
    return se.id;
}

std::string SessionManager::Extract_ID(const std::string& cookie)
{
    std::size_t pos = cookie.find("session_id=");
    if (pos == std::string::npos)
        return std::string();
    pos += 11;
    std::size_t end_pos = cookie.find(";", pos);
    if (end_pos == std::string::npos)
        return cookie.substr(pos);

    return cookie.substr(pos, end_pos - pos);
}
std::string SessionManager::buildCookieHeader(const std::string& ID)
{
    return "session_id=" + ID + "; Path=/; HttpOnly";
}

std::string SessionManager::genarate_ID(){
    static bool Seeds = true;
    if (Seeds)
    {
        std::srand(static_cast<unsigned int>(std::time(NULL)));
        Seeds = false;
    }
    std::string hex = "0123456789abcdef";
    std::string id;
    id.reserve(32);

    for(int i = 0; i < 32; i++){
        id += hex[rand() % 16];
    }
    return id;
}