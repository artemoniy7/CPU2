#pragma once

#include "ServerState.h"

#include <string>
#include <vector>

class ServerShell {
public:
    explicit ServerShell(ServerState& state);
    void run();

private:
    void execute(const std::string& line);
    void printHelp() const;
    void printStatus() const;
    void printServices() const;
    void executeService(const std::vector<std::string>& args);
    static std::vector<std::string> tokenize(const std::string& line);
    static std::string upper(std::string value);

    ServerState& state_;
};
