#include "server/game_server.h"
#include <iostream>

int main() {
    try {
        GameServer server("config.yaml");
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
