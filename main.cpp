#include "include/TrpJson.hpp"
#include "include/TrpSchema.hpp"
#include "config/configSchema.cpp"

#include "server/core/Config.hpp"
#include "server/core/Server.hpp"

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
        return 1;
    }

    TrpValidatorContext ctx;
    if (!serversConfigArray.validate(parser.getAST(), ctx)) {
        std::cerr << "\n--- Validation Errors ---" << std::endl;
        parser.prettyPrint();
        ctx.printErrors();
        return 1;
    }

    try {
        Config configs(parser.getAST());
        // configs.prettyPrint();
        
        Server server(configs);
        server.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
