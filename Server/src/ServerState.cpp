#include "ServerState.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace {
constexpr std::size_t MaxMemoryLogEntries = 100;

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
    if (hostname.empty() || hostname.size() > 32) return false;
    const bool valid = std::all_of(hostname.begin(), hostname.end(),
        [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
    if (!valid) return false;
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

void ServerState::setLogFile(const std::string& path) {
    logFilePath_ = path;
    loggingAvailable_ = true;
    loadRecentLog();
}

const std::string& ServerState::logFilePath() const noexcept { return logFilePath_; }
bool ServerState::loggingAvailable() const noexcept { return loggingAvailable_; }

void ServerState::loadRecentLog() {
    std::ifstream input(logFilePath_);
    if (!input) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(logFilePath_, ec);
        if (ec || exists) {
            loggingAvailable_ = false;
            return;
        }
        const auto parent = std::filesystem::path(logFilePath_).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent, ec);
        if (ec) loggingAvailable_ = false;
        return;
    }

    std::vector<std::string> recent;
    std::string line;
    while (std::getline(input, line)) {
        recent.push_back(line);
        if (recent.size() > MaxMemoryLogEntries) recent.erase(recent.begin());
    }
    if (input.bad()) {
        loggingAvailable_ = false;
        return;
    }
    log_ = std::move(recent);
}

bool ServerState::appendLogLine(const std::string& line) {
    if (!loggingAvailable_) return false;
    std::error_code ec;
    const auto parent = std::filesystem::path(logFilePath_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        loggingAvailable_ = false;
        return false;
    }
    std::ofstream output(logFilePath_, std::ios::app);
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
    return true;
}

void ServerState::addLog(const std::string& message) {
    const std::string line = timestampNow() + " " + message;
    log_.push_back(line);
    if (log_.size() > MaxMemoryLogEntries) log_.erase(log_.begin());
    appendLogLine(line);
}

bool ServerState::clearLog() {
    std::ofstream output(logFilePath_, std::ios::trunc);
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
