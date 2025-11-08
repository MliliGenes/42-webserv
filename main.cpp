#include "include/TrpJson.hpp"
#include "include/TrpSchema.hpp"

int main (int ac, char ** av) {
    if (ac != 2) return 1;
    TrpJsonParser parser(av[1]);

    if (!parser.parse()) {
        std::cerr << "Failed to parse JSON file." << std::endl;
        return 1;
    }

    TrpSchemaFactory factory;
    TrpSchemaObject& serverConfigs = factory.object()
        .property("server",
            &factory.object()
            .property("host", &factory.string()).required("host")
            .property("port", &factory.number()).required("port")
        );
    
    TrpSchemaArray serversConfigs = factory.array().item(&serverConfigs).min(1);

    TrpValidatorContext ctx;
    if (!serversConfigs.validate(parser.getAST(), ctx)) {
        ctx.printErrors();
    } else {
        std::cout << "Configuration is valid!" << std::endl;
    }

    return 0;
}