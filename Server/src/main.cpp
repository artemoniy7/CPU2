#include "ConfigManager.h"
#include "ServerShell.h"
#include "ServerState.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    ServerMode mode = ServerMode::Standalone;
    std::string configPath = "config/server.ini";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--attached") mode = ServerMode::Attached;
        else if (arg == "--standalone") mode = ServerMode::Standalone;
        else if (arg == "--config" && i + 1 < argc) configPath = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            std::cout << "CPU-16 Server OS\nUsage: cpu_server [--standalone|--attached] [--config <path>]\n";
            return 0;
        } else {
            std::cerr << "Unknown option or missing value: " << arg << "\n";
            return 2;
        }
    }

    ServerState state(mode);
    ConfigManager config(configPath);
    std::string configMessage;
    if (!config.load(state, configMessage)) {
        std::cerr << "Configuration warning: " << configMessage << "\n";
    } else if (!configMessage.empty()) {
        std::cout << configMessage << "\n";
    }
    state.addLog("System booted in " + state.modeName() + " mode.");
    if (!state.loggingAvailable()) {
        std::cerr << "Warning: persistent event logging is unavailable.\n";
    }

    ServerShell shell(state, config);
    shell.run();
    return 0;
}
