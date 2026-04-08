#include "include/TrpJson.hpp"
#include "include/TrpSchema.hpp"
#include "config/configSchema.cpp"

#include "server/core/Config.hpp"

int main (int ac, char ** av) {
    TrpJsonParser parser;

    TrpJsonLexer* lexer = NULL;
    if (ac == 1){
        lexer = new TrpJsonLexer("config/minimal.config.json");
    }
    else {
        lexer = new TrpJsonLexer(av[1]);
    }
    parser.setLexer(lexer);

    if (!parser.parse()) {
        std::cerr << "Failed to parse JSON file." << std::endl;
        delete lexer;
        return 1;
    }

    parser.prettyPrint();

    TrpValidatorContext ctx;
    if (!serversConfigArray.validate(parser.getAST(), ctx)) {
        ctx.printErrors();
        return 1;
    }

    try {
        Config configs(parser.getAST());
        // * start(); this should do everything
    } catch (std::exception &e) {
        
    }

    return 0;
}
