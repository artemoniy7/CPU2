#include "ServerShell.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

ServerShell::ServerShell(ServerState& state) : state_(state) {}

void ServerShell::run() {
    std::cout << "\nCPU-16 SERVER OS [development build]\n";
    std::cout << "Copyright (c) CPU2 Project\n";
    std::cout << "Boot mode: " << state_.modeName() << "\n";
    std::cout << "Type HELP for available commands.\n\n";

    std::string line;
    while (state_.running()) {
        std::cout << "CPU16> ";
        if (!std::getline(std::cin, line)) {
            state_.requestShutdown();
            break;
        }
        execute(line);
    }
}

void ServerShell::execute(const std::string& input) {
    std::string command = input;
    std::transform(command.begin(), command.end(), command.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    const auto first = command.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    const auto last = command.find_last_not_of(" \t\r\n");
    command = command.substr(first, last - first + 1);

    if (command == "HELP" || command == "?") {
        printHelp();
    } else if (command == "STATUS") {
        printStatus();
    } else if (command == "MACHINES") {
        std::cout << "Machine registry is not connected yet.\n";
    } else if (command == "SERVICES" || command == "LIST") {
        std::cout << "No services loaded. Service Manager is planned.\n";
    } else if (command == "LOG") {
        std::cout << "Logging subsystem is not initialized yet.\n";
    } else if (command == "SHUTDOWN" || command == "EXIT" || command == "QUIT") {
        std::cout << "Shutting down CPU-16 Server OS...\n";
        state_.requestShutdown();
    } else {
        std::cout << "Bad command or file name: " << input << "\n";
        std::cout << "Type HELP for a list of commands.\n";
    }
}

void ServerShell::printHelp() const {
    std::cout << "\nCPU-16 SERVER COMMANDS\n"
              << " HELP       Display this help\n"
              << " STATUS     Show server status\n"
              << " MACHINES   List virtual machines (planned)\n"
              << " SERVICES   List system services (planned)\n"
              << " LOG        Show system log (planned)\n"
              << " SHUTDOWN   Stop the server\n\n";
}

void ServerShell::printStatus() const {
    std::cout << "\nCPU-16 SYSTEM STATUS\n"
              << " Mode:       " << state_.modeName() << "\n"
              << " State:      ONLINE\n"
              << " CPU Core:   NOT CONNECTED\n"
              << " IPC:        NOT INITIALIZED\n"
              << " Services:   0\n"
              << " Network:    OFFLINE\n\n";
}
