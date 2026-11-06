#include "ServerShell.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

ServerShell::ServerShell(ServerState& state) : state_(state) {}

std::string ServerShell::upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::vector<std::string> ServerShell::tokenize(const std::string& line) {
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string token;
    while (input >> std::quoted(token)) result.push_back(token);
    return result;
}

void ServerShell::run() {
    std::cout << "\nCPU-16 SERVER OS [development build]\n"
              << "Copyright (c) CPU2 Project\n"
              << "Node: " << state_.hostname() << "\n"
              << "Boot mode: " << state_.modeName() << "\n"
              << "Type HELP for available commands.\n\n";

    std::string line;
    while (state_.running()) {
        std::cout << state_.hostname() << "> ";
        if (!std::getline(std::cin, line)) {
            state_.requestShutdown();
            break;
        }
        execute(line);
    }
    std::cout << "CPU-16 Server OS stopped.\n";
}

void ServerShell::execute(const std::string& line) {
    const auto args = tokenize(line);
    if (args.empty()) return;

    const std::string command = upper(args[0]);

    if (command == "HELP" || command == "?") {
        printHelp();
    } else if (command == "STATUS") {
        printStatus();
    } else if (command == "VERSION") {
        std::cout << "CPU-16 Server OS 0.2.0-dev\n";
    } else if (command == "HOSTNAME") {
        if (args.size() == 1) {
            std::cout << state_.hostname() << "\n";
        } else if (args.size() == 2 && state_.setHostname(args[1])) {
            std::cout << "Hostname updated.\n";
        } else {
            std::cout << "Usage: HOSTNAME [new-name] (1-32 letters, digits, '-' or '_')\n";
        }
    } else if (command == "UPTIME") {
        const auto seconds = state_.uptimeSeconds();
        std::cout << "Uptime: " << seconds / 3600 << "h "
                  << (seconds % 3600) / 60 << "m " << seconds % 60 << "s\n";
    } else if (command == "WHOAMI") {
        std::cout << "SYSTEM\\ADMIN (local shell; authentication not implemented)\n";
    } else if (command == "ECHO") {
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (i > 1) std::cout << ' ';
            std::cout << args[i];
        }
        std::cout << "\n";
    } else if (command == "CLEAR" || command == "CLS") {
        std::cout << "\033[2J\033[H";
    } else if (command == "MACHINES") {
        std::cout << "No virtual machine is registered. CPU Core integration is pending.\n";
    } else if (command == "SERVICES" || command == "LIST") {
        printServices();
    } else if (command == "SERVICE") {
        executeService(args);
    } else if (command == "LOG") {
        const auto& entries = state_.log();
        if (entries.empty()) std::cout << "Log is empty.\n";
        for (const auto& entry : entries) std::cout << "[SYSTEM] " << entry << "\n";
    } else if (command == "NETSTAT") {
        std::cout << "Network subsystem is OFFLINE (no sockets are opened).\n";
    } else if (command == "PROCESS" || command == "TASKLIST") {
        std::cout << "No guest processes: CPU Core and guest loader are not connected.\n";
    } else if (command == "SHUTDOWN" || command == "EXIT" || command == "QUIT") {
        std::cout << "Shutting down CPU-16 Server OS...\n";
        state_.requestShutdown();
    } else {
        std::cout << "Bad command or file name: " << args[0] << "\n"
                  << "Type HELP for a list of commands.\n";
    }
}

void ServerShell::printHelp() const {
    std::cout << "\nCPU-16 SERVER COMMANDS\n"
              << " HELP                 Display this help\n"
              << " STATUS               Show system status\n"
              << " VERSION              Show server version\n"
              << " HOSTNAME [name]      Show or change node name\n"
              << " UPTIME               Show server uptime\n"
              << " WHOAMI               Show current shell identity\n"
              << " ECHO <text>          Print text\n"
              << " CLEAR                Clear terminal (ANSI)\n"
              << " MACHINES             List virtual machines\n"
              << " SERVICES              List services\n"
              << " SERVICE START <name> Start a service entry\n"
              << " SERVICE STOP <name>  Stop a service entry\n"
              << " SERVICE INFO <name>  Show service details\n"
              << " PROCESS              List guest processes\n"
              << " NETSTAT              Show network subsystem status\n"
              << " LOG                  Show recent system events\n"
              << " SHUTDOWN             Stop the server\n\n";
}

void ServerShell::printStatus() const {
    std::size_t active = 0;
    for (const auto& service : state_.services()) if (service.running) ++active;

    std::cout << "\nCPU-16 SYSTEM STATUS\n"
              << " Hostname:   " << state_.hostname() << "\n"
              << " Mode:       " << state_.modeName() << "\n"
              << " State:      ONLINE\n"
              << " Uptime:     " << state_.uptimeSeconds() << " seconds\n"
              << " CPU Core:   NOT CONNECTED\n"
              << " IPC:        NOT INITIALIZED\n"
              << " Services:   " << active << "/" << state_.services().size() << " active\n"
              << " Network:    OFFLINE\n\n";
}

void ServerShell::printServices() const {
    std::cout << "\nSERVICE       STATE       TYPE\n"
              << "----------------------------------------\n";
    for (const auto& service : state_.services()) {
        std::cout << std::left << std::setw(14) << service.name
                  << std::setw(12) << (service.running ? "RUNNING" : "STOPPED")
                  << (service.implemented ? "BUILT-IN" : "PLACEHOLDER") << "\n";
    }
    std::cout << "\nNote: service states are managed locally; no guest code or network daemon is launched yet.\n";
}

void ServerShell::executeService(const std::vector<std::string>& args) {
    if (args.size() != 3) {
        std::cout << "Usage: SERVICE <START|STOP|INFO> <name>\n";
        return;
    }

    const std::string action = upper(args[1]);
    ServiceInfo* service = state_.findService(args[2]);
    if (!service) {
        std::cout << "Service not found: " << args[2] << "\n";
        return;
    }

    if (action == "INFO") {
        std::cout << "Name: " << service->name << "\n"
                  << "Description: " << service->description << "\n"
                  << "State: " << (service->running ? "RUNNING" : "STOPPED") << "\n"
                  << "Implementation: " << (service->implemented ? "BUILT-IN" : "PLACEHOLDER") << "\n";
    } else if (action == "START" || action == "STOP") {
        if (!service->implemented) {
            std::cout << service->name << " is a placeholder. Its executable is not implemented yet.\n";
            return;
        }
        const bool start = action == "START";
        if (service->running == start) {
            std::cout << service->name << " is already " << (start ? "running" : "stopped") << ".\n";
            return;
        }
        service->running = start;
        state_.addLog(service->name + (start ? " started." : " stopped."));
        std::cout << service->name << (start ? " started.\n" : " stopped.\n");
    } else {
        std::cout << "Unknown service action. Use START, STOP or INFO.\n";
    }
}
