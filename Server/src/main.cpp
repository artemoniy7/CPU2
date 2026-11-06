#include "ServerShell.h"
#include "ServerState.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    ServerMode mode = ServerMode::Standalone;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--attached") {
            mode = ServerMode::Attached;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "CPU-16 Server OS\n"
                      << "Usage: cpu_server [--standalone|--attached]\n";
            return 0;
        } else if (arg != "--standalone") {
            std::cerr << "Unknown option: " << arg << "\n";
            return 2;
        }
    }

    ServerState state(mode);
    ServerShell shell(state);
    shell.run();
    return 0;
}
