DATE := $(shell date +"%Y-%m-%d %H:%M:%S")
TOTAL_FILES = $(words $(ALL_SRC))

CURRENT_FILE = 0

define print_progress
$(eval CURRENT_FILE=$(shell echo $$(($(CURRENT_FILE) + 1))))
$(eval PERCENT=$(shell echo $$(($(CURRENT_FILE) * 100 / $(TOTAL_FILES)))))
echo "[$(DATE)] [Compiling] $< → $@ - $(PERCENT)% complete"
endef

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

ROOT_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(ROOT_SRC))

ALL_OBJ = $(ROOT_OBJ) 

all: $(NAME)

re: clean all

clean:
	@rm -fr build

fclean: clean
	@rm -fr $(NAME)

$(NAME): $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB) $(ALL_OBJ)
	@$(CXX) -o $(NAME) $(CXX_FLAGS) $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB) $(ALL_OBJ)
	@$(print_progress)

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

.PHONY: all clean