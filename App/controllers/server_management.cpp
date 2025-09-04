#include "server_management.h"

#include <crow.h>

namespace server_management {
    void start_server() {
        crow::SimpleApp app;

        // Define a simple route
        CROW_ROUTE(app, "/")([](){
            return "Hello, Crow!";
        });

        uint16_t server_port = 8080;

        app.port(server_port).multithreaded().run();
    }
}