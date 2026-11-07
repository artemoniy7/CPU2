#include "ConfigManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool validHostname(const std::string& value) {
    if (value.empty() || value.size() > 32) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') ||
               (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9') ||
               ch == '-' || ch == '_';
    });
}

bool validLogPath(const std::string& value) {
    if (value.empty() || value.size() > 240) return false;
    return std::none_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch < 0x20 || ch == 0x7f;
    });
}

bool replaceFile(const std::filesystem::path& temporary,
                 const std::filesystem::path& destination,
                 std::string& error) {
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    const auto code = GetLastError();
    error = "Cannot replace configuration file: " +
            std::system_category().message(static_cast<int>(code));
    return false;
#else
    std::error_code ec;
    std::filesystem::rename(temporary, destination, ec);
    if (ec) {
        error = "Cannot replace configuration file: " + ec.message();
        return false;
    }
    return true;
#endif
}
}

ConfigManager::ConfigManager(std::string path) : path_(std::move(path)) {}

const std::string& ConfigManager::path() const noexcept { return path_; }

bool ConfigManager::load(ServerState& state, std::string& error) {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path_, ec);
        if (ec || exists) {
            error = "Cannot open configuration file: " + path_;
            return false;
        }

        std::string logError;
        if (!state.setLogFile(state.logFilePath(), logError)) {
            error = "Cannot initialize persistent event log: " + logError;
            return false;
        }
        if (!save(state, error)) return false;
        error = "Configuration file was missing; defaults were created at " + path_;
        return true;
    }

    std::string hostname = state.hostname();
    std::string logFile = state.logFilePath();
    bool sawHostname = false;
    bool sawLogFile = false;
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
            if (sawHostname) {
                error = "Duplicate hostname key at line " + std::to_string(lineNumber);
                return false;
            }
            sawHostname = true;
            hostname = value;
        } else if (key == "log_file") {
            if (sawLogFile) {
                error = "Duplicate log_file key at line " + std::to_string(lineNumber);
                return false;
            }
            sawLogFile = true;
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

    if (!validHostname(hostname)) {
        error = "Invalid hostname in configuration (1-32 ASCII letters, digits, '-' or '_').";
        return false;
    }
    if (!validLogPath(logFile)) {
        error = "Invalid log_file value in configuration (1-240 characters, no control characters).";
        return false;
    }

    std::string logError;
    if (!state.setLogFile(logFile, logError)) {
        error = "Cannot activate configured log file '" + logFile + "': " + logError;
        return false;
    }
    if (!state.setHostname(hostname)) {
        error = "Invalid hostname in configuration.";
        return false;
    }

    error.clear();
    state.addLog("Configuration loaded from " + path_ + ".");
    return true;
}

bool ConfigManager::save(const ServerState& state, std::string& error) const {
    if (path_.empty()) {
        error = "Configuration path is empty.";
        return false;
    }
    if (!validHostname(state.hostname())) {
        error = "Cannot save configuration: hostname is invalid.";
        return false;
    }
    if (!validLogPath(state.logFilePath())) {
        error = "Cannot save configuration: log file path is invalid.";
        return false;
    }

    const std::filesystem::path destination(path_);
    const auto parent = destination.parent_path();
    std::error_code ec;
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        error = "Cannot create configuration directory: " + ec.message();
        return false;
    }

    std::filesystem::path temporary = destination;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Cannot write temporary configuration file: " + temporary.string();
            return false;
        }
        output << "# CPU-16 Server OS configuration\n"
               << "hostname=" << state.hostname() << "\n"
               << "log_file=" << state.logFilePath() << "\n";
        output.flush();
        if (!output) {
            error = "Failed while writing configuration file: " + temporary.string();
            output.close();
            std::error_code cleanupError;
            std::filesystem::remove(temporary, cleanupError);
            return false;
        }
        output.close();
        if (!output) {
            error = "Failed while closing configuration file: " + temporary.string();
            std::error_code cleanupError;
            std::filesystem::remove(temporary, cleanupError);
            return false;
        }
    }

    if (!replaceFile(temporary, destination, error)) {
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }

    error.clear();
    return true;
}