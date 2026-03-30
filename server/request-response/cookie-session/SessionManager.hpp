#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <string>
#include <ctime>
#include <map>

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

        session*    get(const std::string& ID);
        std::string create();

        static std::string Extract_ID(const std::string& cookie);
        static std::string buildCookieHeader(const std::string& ID);

    private:
        std::map<std::string, session> sessions; // <ID , session>

        std::string genarate_ID();
};


#endif