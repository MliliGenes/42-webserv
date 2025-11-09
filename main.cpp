#include "include/TrpJson.hpp"
#include "include/TrpSchema.hpp"
#include "config/configSchema.cpp"

int main (int ac, char ** av) {
    TrpJsonParser parser;
    if (ac == 1)
        parser.setLexer(new TrpJsonLexer("./config/minimal.config.json"));
    else
        parser.setLexer(new TrpJsonLexer(av[1]));

    if (!parser.parse()) {
        std::cerr << "Failed to parse JSON file." << std::endl;
        return 1;
    }

    TrpValidatorContext ctx;
    if (!serversConfigArray.validate(parser.getAST(), ctx)) {
        ctx.printErrors();
        return 1;
    }

    return 0;
}
