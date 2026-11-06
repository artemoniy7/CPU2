#include "ServerState.h"

#include <algorithm>
#include <cctype>

ServerState::ServerState(ServerMode mode)
    : mode_(mode), startedAt_(std::chrono::steady_clock::now()) {
    services_.push_back({"CLOCK", "System clock service", true, true});
    services_.push_back({"SPOOLER", "Print queue service (placeholder)", false, false});
    services_.push_back({"HTTPD", "HTTP server service (placeholder)", false, false});
    services_.push_back({"FTPD", "File transfer service (placeholder)", false, false});
    addLog("System booted in " + modeName() + " mode.");
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
    hostname_ = hostname;
    addLog("Hostname changed to " + hostname_ + ".");
    return true;
}

long long ServerState::uptimeSeconds() const noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startedAt_).count();
}

const std::vector<ServiceInfo>& ServerState::services() const noexcept {
    return services_;
}

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

void ServerState::addLog(const std::string& message) {
    log_.push_back(message);
    if (log_.size() > 100) log_.erase(log_.begin());
}

const std::vector<std::string>& ServerState::log() const noexcept { return log_; }
