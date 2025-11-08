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
CXX_FLAGS = -Wall -Wextra -Werror -ggdb -std=c++98

BUILD_DIR = build

TRP_JSON_DIR = TrpJSON
TRP_SCHEMA_DIR = TrpSchema

TRP_JSON_LIB := $(TRP_JSON_DIR)/lib/libtrpjson.a
TRP_SCHEMA_LIB := $(TRP_SCHEMA_DIR)/lib/libtrpschema.a

TRP_JSON_SRC := $(wildcard $(TRP_JSON_DIR)/src/*.cpp)
TRP_SCHEMA_SRC := $(wildcard $(TRP_SCHEMA_DIR)/src/*.cpp)

ROOT_DIR = .
CORE_DIR = 
HTTP_REQ_DIR = 
HTTP_RES_DIR = 
CGI_DIR = 

ROOT_SRC = $(wildcard *.cpp)

ALL_SRC = $(ROOT_SRC)

ROOT_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(ROOT_SRC))
# each dir will have its .o files

ALL_OBJ = $(ROOT_OBJ) #more will go here

all: $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB) $(NAME)

re: clean all

clean:
	@rm -fr build

fclean: clean
	@rm -fr $(NAME)

$(NAME): $(ALL_OBJ) 
	@$(CXX) -o $(NAME) $(CXX_FLAGS) $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB) $(ALL_OBJ)
	@$(print_progress)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXX_FLAGS) -c $< -o $@
	@$(print_progress)

$(TRP_JSON_LIB): $(TRP_JSON_SRC)
	@make lib -C $(TRP_JSON_DIR)
	@echo $(TRP_JSON_LIB) "is ready for use!"

$(TRP_SCHEMA_LIB): $(TRP_SCHEMA_SRC)
	@make lib -C $(TRP_SCHEMA_DIR)
	@echo $(TRP_JSON_LIB) "is ready for use!"

.PHONY: all clean