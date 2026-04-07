DATE := $(shell date +"%Y-%m-%d %H:%M:%S")

NAME = webserv

CXX = c++
CXX_FLAGS = -Wall -Wextra -Werror -std=c++98

BUILD_DIR = build

TRP_JSON_DIR = dependencies/TrpJSON
TRP_SCHEMA_DIR = dependencies/TrpSchema

TRP_JSON_LIB = $(TRP_JSON_DIR)/lib/libtrpjson.a
TRP_SCHEMA_LIB = $(TRP_SCHEMA_DIR)/lib/libtrpschema.a

ROOT_DIR = .
CORE_DIR = 
HTTP_REQ_DIR = 
HTTP_RES_DIR = 
CGI_DIR = 

ROOT_SRC = $(wildcard *.cpp)
SERVER_SRC = $(wildcard server/core/*.cpp) \
		$(wildcard server/cgi/implementation/*.cpp) \
		$(wildcard server/request-response/request/*.cpp) \
		$(wildcard server/request-response/response/*.cpp) \
		$(wildcard server/request-response/cookie-session/*.cpp)

SRC = $(ROOT_SRC) $(SERVER_SRC)

ALL_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC))

TOTAL_FILES = $(words $(ALL_OBJ))

CURRENT_FILE = 0

define print_progress
$(eval CURRENT_FILE=$(shell echo $$(($(CURRENT_FILE) + 1))))
$(eval PERCENT=$(shell echo $$(($(CURRENT_FILE) * 100 / $(TOTAL_FILES)))))
echo "[$(DATE)] [Compiling] $< → $@ - $(PERCENT)% complete"
endef


all: $(NAME)

re: clean all

clean: fclean_libs
	@rm -fr build

fclean: clean
	@rm -fr $(NAME)

fclean_libs:
	@$(MAKE) -C $(TRP_JSON_DIR) fclean
	@$(MAKE) -C $(TRP_SCHEMA_DIR) fclean
	@echo "[$(DATE)] [Cleaned] Dependencies"

$(NAME): $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB) $(ALL_OBJ)
	@$(CXX) $(CXX_FLAGS) -o $(NAME) $(ALL_OBJ) $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB)
	@echo "[$(DATE)] [Built] $@ - webserv is ready for use!"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(TRP_JSON_LIB):
	@make lib -C $(TRP_JSON_DIR)
	@echo $(TRP_JSON_LIB) "is ready for use!"

$(TRP_SCHEMA_LIB):
	@make lib -C $(TRP_SCHEMA_DIR)
	@echo $(TRP_JSON_LIB) "is ready for use!"

.PHONY: all clean fclean re fclean_libs