#include "ServerState.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace {
constexpr std::size_t MaxMemoryLogEntries = 100;

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

std::string timestampNow() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return buffer;
}
}

ServerState::ServerState(ServerMode mode)
    : mode_(mode), startedAt_(std::chrono::steady_clock::now()) {
    services_.push_back({"CLOCK", "System clock service", true, true});
    services_.push_back({"SPOOLER", "Print queue service (placeholder)", false, false});
    services_.push_back({"HTTPD", "HTTP server service (placeholder)", false, false});
    services_.push_back({"FTPD", "File transfer service (placeholder)", false, false});
}

ServerMode ServerState::mode() const noexcept { return mode_; }
bool ServerState::running() const noexcept { return running_; }

void ServerState::requestShutdown() {
    addLog("Shutdown requested.");
    running_ = false;
}

std::string ServerState::modeName() const {
    return mode_ == ServerMode::Standalone ? "STANDALONE" : "ATTACHED";
}

const std::string& ServerState::hostname() const noexcept { return hostname_; }

bool ServerState::setHostname(const std::string& hostname) {
    if (!validHostname(hostname)) return false;
    if (hostname_ == hostname) return true;
    hostname_ = hostname;
    addLog("Hostname changed to " + hostname_ + ".");
    return true;
}

long long ServerState::uptimeSeconds() const noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startedAt_).count();
}

const std::vector<ServiceInfo>& ServerState::services() const noexcept { return services_; }

ServiceInfo* ServerState::findService(const std::string& name) {
    for (auto& service : services_) {
        std::string a = service.name;
        std::string b = name;
        std::transform(a.begin(), a.end(), a.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        std::transform(b.begin(), b.end(), b.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (a == b) return &service;
    }
    return nullptr;
}

bool ServerState::setLogFile(const std::string& path, std::string& error) {
    if (!validLogPath(path)) {
        error = "Log path must contain 1-240 characters and no control characters.";
        return false;
    }

    const std::filesystem::path filePath(path);
    const auto parent = filePath.parent_path();
    std::error_code ec;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "Cannot create log directory: " + ec.message();
            return false;
        }
    }

    std::deque<std::string> recent;
    std::ifstream input(filePath, std::ios::binary);
    if (input) {
        std::string line;
        while (std::getline(input, line)) {
            recent.push_back(line);
            if (recent.size() > MaxMemoryLogEntries) recent.pop_front();
        }
        if (input.bad()) {
            error = "Error while reading log file: " + path;
            return false;
        }
    } else {
        ec.clear();
        const bool exists = std::filesystem::exists(filePath, ec);
        if (ec || exists) {
            error = "Cannot read existing log file: " + path;
            return false;
        }
    }

    // Probe append access before changing the active path or in-memory history.
    std::ofstream probe(filePath, std::ios::app | std::ios::binary);
    if (!probe) {
        error = "Cannot open log file for appending: " + path;
        return false;
    }
    probe.flush();
    if (!probe) {
        error = "Cannot flush log file: " + path;
        return false;
    }
    probe.close();
    if (!probe) {
        error = "Cannot close log file cleanly: " + path;
        return false;
    }

    logFilePath_ = path;
    log_.assign(recent.begin(), recent.end());
    loggingAvailable_ = true;
    error.clear();
    return true;
}

const std::string& ServerState::logFilePath() const noexcept { return logFilePath_; }
bool ServerState::loggingAvailable() const noexcept { return loggingAvailable_; }

bool ServerState::appendLogLine(const std::string& line) {
    if (!loggingAvailable_) return false;

    std::error_code ec;
    const auto parent = std::filesystem::path(logFilePath_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        loggingAvailable_ = false;
        return false;
    }

    std::ofstream output(logFilePath_, std::ios::app | std::ios::binary);
    if (!output) {
        loggingAvailable_ = false;
        return false;
    }
    output << line << '\n';
    output.flush();
    if (!output) {
        loggingAvailable_ = false;
        return false;
    }
    output.close();
    if (!output) {
        loggingAvailable_ = false;
        return false;
    }
    return true;
}

void ServerState::addLog(const std::string& message) {
    std::string safeMessage = message;
    std::replace(safeMessage.begin(), safeMessage.end(), '\r', ' ');
    std::replace(safeMessage.begin(), safeMessage.end(), '\n', ' ');

    const std::string line = timestampNow() + " " + safeMessage;
    log_.push_back(line);
    if (log_.size() > MaxMemoryLogEntries) log_.erase(log_.begin());
    appendLogLine(line);
}

bool ServerState::clearLog() {
    std::error_code ec;
    const auto parent = std::filesystem::path(logFilePath_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        loggingAvailable_ = false;
        return false;
    }

    std::ofstream output(logFilePath_, std::ios::trunc | std::ios::binary);
    if (!output) {
        loggingAvailable_ = false;
        return false;
    }
    output.flush();
    if (!output) {
        loggingAvailable_ = false;
        return false;
    }
    output.close();
    if (!output) {
        loggingAvailable_ = false;
        return false;
    }

    log_.clear();
    loggingAvailable_ = true;
    addLog("Event log cleared.");
    return loggingAvailable_;
}

const std::vector<std::string>& ServerState::log() const noexcept { return log_; }