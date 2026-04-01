#include "SessionManager.hpp"

SessionManager::SessionManager()
{

}

session*    SessionManager::get(const std::string& ID)
{
    std::map<std::string, session>::const_iterator it;
    it = sessions_.find(ID);
    if (it == sessions_.end())
        return NULL;
    if (std::time(NULL) - it->second.last_access > SESSION_TTL){
        sessions_.erase(ID);
        return NULL;
    }
    session s = it->second;
    return &s;
}
std::string SessionManager::create()
{
    session se;

    se.id = genarate_ID();
    sessions_[se.id] = se;
    return se.id;
}

std::string SessionManager::Extract_ID(const std::string& cookie)
{
    std::size_t pos = cookie.find("session_id=");
    if (pos == std::string::npos)
        return std::string();
    std::size_t end_pos = cookie.find(";", pos + 11);
    if (end_pos == std::string::npos)
        return cookie.substr(pos);

    std::string key = cookie.substr(pos, end_pos - pos);
    return key;
}
std::string SessionManager::buildCookieHeader(const std::string& ID)
{
    return "session_id=" + ID + "; Path=/; HttpOnly";
}

std::string SessionManager::genarate_ID(){
    static bool Seeds = true;
    if (Seeds)
    {
        srand(std::time(NULL));
        Seeds = false;
    }
    std::string hex = "123456789abcdef";
    std::string id;
    id.reserve(32);
    std::size_t r = rand();
    for(int i = 0; i < 32; i++){
        
    }

}