#pragma once

#include "ServerState.h"

class ServerShell {
public:
    explicit ServerShell(ServerState& state);
    void run();

private:
    void execute(const std::string& command);
    void printHelp() const;
    void printStatus() const;

    ServerState& state_;
};
