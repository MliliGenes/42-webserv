#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <string>
#include <ctime>
#include <map>


#include <sstream>


struct session
{
    std::string id;
    std::map<std::string, std::string> data;
    std::time_t time_creatation;
    std::time_t last_access;

    session() : time_creatation(std::time(NULL)), last_access(std::time(NULL)) {}
};

class SessionManager
{

    public:

        static const int SESSION_TTL = 1800;
        SessionManager();

        session*    get(const std::string& ID);
        std::string create();

        static std::string Extract_ID(const std::string& cookie);
        static std::string buildCookieHeader(const std::string& ID);

    private:
        
        std::map<std::string, session> sessions_; // <ID , session>

        std::string genarate_ID();
};


#endif