#pragma once

#include "ConfigManager.h"
#include "ServerState.h"

#include <string>
#include <vector>

class ServerShell {
public:
    ServerShell(ServerState& state, ConfigManager& config);
    void run();

private:
    void execute(const std::string& line);
    void printHelp() const;
    void printStatus() const;
    void printServices() const;
    void executeService(const std::vector<std::string>& args);
    void executeConfig(const std::vector<std::string>& args);
    void executeLog(const std::vector<std::string>& args);
    static std::vector<std::string> tokenize(const std::string& line);
    static std::string upper(std::string value);

    ServerState& state_;
    ConfigManager& config_;
};