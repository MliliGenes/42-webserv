#include "../include/TrpSchema.hpp"

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

TrpSchemaObject& serverSchema = factory.object()
    .property("host", &factory.string().min(1))
    .property("port", &factory.number().min(1).max(65535))
    .property("server_name", &factory.array().item(&factory.string().min(1)))
    .property("client_max_body_size", &factory.string())
    .property("error_pages", &errorPagesSchema)
    .property("root", &factory.string())
    .property("index", &factory.array().item(&factory.string()))
    .property("locations", &factory.array().item(&routeSchema))
    .required("host")
    .required("port")
    .required("client_max_body_size");

TrpSchemaObject& serverConfigWrapper = factory.object()
    .property("server", &serverSchema)
    .required("server");

TrpSchemaArray& serversConfigArray = factory.array()
    .item(&serverConfigWrapper)
    .min(1);