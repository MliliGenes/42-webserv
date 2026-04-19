DATE := $(shell date +"%Y-%m-%d %H:%M:%S")

NAME = webserv

CXX = c++
CXX_FLAGS = -Wall -Wextra -Werror #`-std=c++98 -fsanitize=address -g3 -MMD -MP

BUILD_DIR = build

TRP_JSON_DIR = dependencies/TrpJSON
TRP_SCHEMA_DIR = dependencies/TrpSchema

TRP_JSON_LIB = $(TRP_JSON_DIR)/lib/libtrpjson.a
TRP_SCHEMA_LIB = $(TRP_SCHEMA_DIR)/lib/libtrpschema.a

ROOT_DIR = .
CORE_DIR = server/core
COOKIE_DIR = server/request-response/cookie-session
REQ_DIR = server/request-response/request
RES_DIR = server/request-response/response
RES_DIR = server/request-response/response
CGI_DIR = server/cgi/implementation


ROOT_SRC = $(wildcard *.cpp)
CORE_SRC = $(wildcard $(CORE_DIR)/*.cpp)
COOKIE_SRC = $(wildcard $(COOKIE_DIR)/*.cpp)
REQ_SRC = $(wildcard $(REQ_DIR)/*.cpp)
RES_SRC = $(wildcard $(RES_DIR)/*.cpp)
CGI_SRC = $(wildcard $(CGI_DIR)/*.cpp)

ROOT_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(ROOT_SRC))
CORE_OBJ = $(patsubst $(CORE_DIR)/%.cpp,$(BUILD_DIR)/$(CORE_DIR)/%.o,$(CORE_SRC))
COOKIE_OBJ = $(patsubst $(COOKIE_DIR)/%.cpp,$(BUILD_DIR)/$(COOKIE_DIR)/%.o,$(COOKIE_SRC))
REQ_OBJ = $(patsubst $(REQ_DIR)/%.cpp,$(BUILD_DIR)/$(REQ_DIR)/%.o,$(REQ_SRC))
RES_OBJ = $(patsubst $(RES_DIR)/%.cpp,$(BUILD_DIR)/$(RES_DIR)/%.o,$(RES_SRC))
CGI_OBJ = $(patsubst $(CGI_DIR)/%.cpp,$(BUILD_DIR)/$(CGI_DIR)/%.o,$(CGI_SRC))

ALL_OBJ = $(ROOT_OBJ) $(CORE_OBJ) $(COOKIE_OBJ) $(REQ_OBJ) $(RES_OBJ) $(CGI_OBJ)

TOTAL_FILES = $(words $(ALL_OBJ))

CURRENT_FILE = 0

define print_progress
$(eval CURRENT_FILE=$(shell echo $$(($(CURRENT_FILE) + 1))))
$(eval PERCENT=$(shell echo $$(($(CURRENT_FILE) * 100 / $(TOTAL_FILES)))))
echo "[$(DATE)] [Compiling] $< → $@ - $(PERCENT)% complete"
endef


all: $(NAME)

run: all
	@./webserv config/www.config.json

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

$(BUILD_DIR)/$(CORE_DIR)/%.o: $(CORE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(BUILD_DIR)/$(COOKIE_DIR)/%.o: $(COOKIE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(BUILD_DIR)/$(REQ_DIR)/%.o: $(REQ_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(BUILD_DIR)/$(RES_DIR)/%.o: $(RES_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(BUILD_DIR)/$(CGI_DIR)/%.o: $(CGI_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(TRP_JSON_LIB):
	@make lib -C $(TRP_JSON_DIR)
	@echo $(TRP_JSON_LIB) "is ready for use!"

$(TRP_SCHEMA_LIB):
	@make lib -C $(TRP_SCHEMA_DIR)
	@echo $(TRP_JSON_LIB) "is ready for use!"

# Include header file dependencies
-include $(ALL_OBJ:.o=.d)

.PHONY: all clean fclean re fclean_libs