#include "include/TrpJson.hpp"
#include "include/TrpSchema.hpp"

int main (int ac, char ** av) {
    if (ac != 2) return 1;
    TrpJsonParser parser(av[1]);

    if (!parser.parse()) {
        std::cerr << "Failed to parse JSON file." << std::endl;
        return 1;
    }
    parser.prettyPrint();
    TrpSchemaFactory factory;

    // Error pages schema: {"404": "/path/to/404.html", "500": "/path/to/500.html"}
    TrpSchemaObject& errorPagesSchema = factory.object()
        .property("404", &factory.string())
        .property("403", &factory.string())
        .property("500", &factory.string())
        .property("502", &factory.string())
        .property("503", &factory.string())
        .property("504", &factory.string());

    // CGI configuration: {".php": "/usr/bin/php-cgi", ".py": "/usr/bin/python3"}
    TrpSchemaObject& cgiSchema = factory.object()
        .property(".php", &factory.string())
        .property(".py", &factory.string())
        .property(".js", &factory.string())
        .property(".sh", &factory.string());

    // Redirect schema: {"code": 301, "url": "https://example.com"}
    TrpSchemaObject& redirectSchema = factory.object()
        .property("code", &factory.number().min(300).max(399))
        .property("url", &factory.string().min(1))
        .required("code")
        .required("url");

    // Route schema
    TrpSchemaObject& routeSchema = factory.object()
        .property("path", &factory.string().min(1))
        .property("methods", &factory.array().item(&factory.string()))
        .property("root", &factory.string())
        .property("index", &factory.array().item(&factory.string()))
        .property("autoindex", &factory.boolean())
        .property("redirect", &redirectSchema)
        .property("upload_enable", &factory.boolean())
        .property("upload_store", &factory.string())
        .property("cgi", &cgiSchema)
        .required("path");

    // Server schema
    TrpSchemaObject& serverSchema = factory.object()
        .property("host", &factory.string().min(1))
        .property("port", &factory.number().min(1).max(65535))
        .property("server_name", &factory.string())
        .property("client_max_body_size", &factory.string())
        .property("error_pages", &errorPagesSchema)
        .property("root", &factory.string())
        .property("index", &factory.array().item(&factory.string()))
        .property("routes", &factory.array().item(&routeSchema))
        .required("host")
        .required("port");

    // Wrapper for server config
    TrpSchemaObject& serverConfigWrapper = factory.object()
        .property("server", &serverSchema)
        .required("server");

    // Top-level array of server configs
    TrpSchemaArray& serversConfigArray = factory.array()
        .item(&serverConfigWrapper)
        .min(1);

    TrpValidatorContext ctx;
    if (!serversConfigArray.validate(parser.getAST(), ctx)) {
        std::cerr << "Configuration validation failed:" << std::endl;
        ctx.printErrors();
        return 1;
    } else {
        std::cout << "Configuration is valid!" << std::endl;
    }

    return 0;
}