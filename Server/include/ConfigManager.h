#pragma once

#include "ServerState.h"

#include <string>

class ConfigManager {
public:
    explicit ConfigManager(std::string path);

    bool load(ServerState& state, std::string& error);
    bool save(const ServerState& state, std::string& error) const;
    const std::string& path() const noexcept;

private:
    std::string path_;
};