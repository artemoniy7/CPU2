#pragma once

#include <chrono>
#include <string>
#include <vector>

enum class ServerMode {
    Standalone,
    Attached
};

struct ServiceInfo {
    std::string name;
    std::string description;
    bool running{false};
    bool implemented{false};
};

class ServerState {
public:
    explicit ServerState(ServerMode mode);

    ServerMode mode() const noexcept;
    bool running() const noexcept;
    void requestShutdown() noexcept;
    std::string modeName() const;

    const std::string& hostname() const noexcept;
    bool setHostname(const std::string& hostname);
    long long uptimeSeconds() const noexcept;

    const std::vector<ServiceInfo>& services() const noexcept;
    ServiceInfo* findService(const std::string& name) noexcept;
    void addLog(const std::string& message);
    const std::vector<std::string>& log() const noexcept;

private:
    ServerMode mode_;
    bool running_{true};
    std::string hostname_{"CPU16-SERVER"};
    std::chrono::steady_clock::time_point startedAt_;
    std::vector<ServiceInfo> services_;
    std::vector<std::string> log_;
};
