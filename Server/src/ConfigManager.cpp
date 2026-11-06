#include "ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <system_error>

namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool validLogPath(const std::string& value) {
    if (value.empty() || value.size() > 240) return false;
    return value.find_first_of("\r\n") == std::string::npos;
}
}

ConfigManager::ConfigManager(std::string path) : path_(std::move(path)) {}

const std::string& ConfigManager::path() const noexcept { return path_; }

bool ConfigManager::load(ServerState& state, std::string& error) {
    std::ifstream input(path_);
    if (!input) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path_, ec);
        if (ec || exists) {
            error = "Cannot open configuration file: " + path_;
            return false;
        }
        if (!save(state, error)) return false;
        error = "Configuration file was missing; defaults were created at " + path_;
        return true;
    }

    std::string hostname = state.hostname();
    std::string logFile = state.logFilePath();
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            error = "Invalid configuration syntax at line " + std::to_string(lineNumber);
            return false;
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key == "hostname") {
            hostname = value;
        } else if (key == "log_file") {
            logFile = value;
        } else {
            error = "Unknown configuration key at line " + std::to_string(lineNumber) + ": " + key;
            return false;
        }
    }
    if (input.bad()) {
        error = "Error while reading configuration file: " + path_;
        return false;
    }

    if (!validLogPath(logFile)) {
        error = "Invalid log_file value in configuration.";
        return false;
    }
    if (!state.setHostname(hostname)) {
        error = "Invalid hostname in configuration (1-32 letters, digits, '-' or '_').";
        return false;
    }
    state.setLogFile(logFile);
    if (!state.loggingAvailable()) {
        error = "Configuration loaded, but the log file cannot be accessed: " + logFile;
        return false;
    }
    error.clear();
    state.addLog("Configuration loaded from " + path_ + ".");
    return true;
}

bool ConfigManager::save(const ServerState& state, std::string& error) const {
    std::error_code ec;
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        error = "Cannot create configuration directory: " + ec.message();
        return false;
    }

    const std::string temporary = path_ + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            error = "Cannot write temporary configuration file: " + temporary;
            return false;
        }
        output << "# CPU-16 Server OS configuration\n"
               << "hostname=" << state.hostname() << "\n"
               << "log_file=" << state.logFilePath() << "\n";
        output.flush();
        if (!output) {
            error = "Failed while writing configuration file: " + temporary;
            return false;
        }
    }

    std::filesystem::rename(temporary, path_, ec);
    if (ec) {
        // Windows rename does not replace an existing destination.
        std::error_code removeError;
        std::filesystem::remove(path_, removeError);
        ec.clear();
        std::filesystem::rename(temporary, path_, ec);
    }
    if (ec) {
        error = "Cannot replace configuration file: " + ec.message();
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }
    error.clear();
    return true;
}
