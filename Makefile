NAME = webserv

TRP_JSON_DIR = TrpJSON
TRP_SCHEMA_DIR = TrpSchema

TRP_JSON_LIB = $(wildcard $(TRP_JSON_DIR)/lib/*.a)
TRP_SCHEMA_LIB = $(wildcard $(TRP_SCHEMA_DIR)/lib/*.a)

all: trp-json trp-schema
	@echo $(TRP_JSON_LIB) $(TRP_SCHEMA_LIB)

trp-json:
	@make lib-re -C $(TRP_JSON_DIR)

trp-schema:
	@make lib-re -C $(TRP_SCHEMA_DIR)