NAME    = webserv
CXX     = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS =  main.cpp \
        Config.cpp \
        Server.cpp \
        ../../RequestParser.cpp \
        ../../ResponseBuilder.cpp \
        ../../SessionManager.cpp \
        ../../../cgi/implementation/Cgi.cpp \

OBJS = $(SRCS:.cpp=.o)

INCLUDES = -I. -I../include \
           -I../include \
           -I../../../cgi/include

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
