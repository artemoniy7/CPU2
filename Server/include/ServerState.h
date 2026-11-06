#pragma once

#include <string>

enum class ServerMode {
    Standalone,
    Attached
};

class ServerState {
public:
    explicit ServerState(ServerMode mode);

    ServerMode mode() const noexcept;
    bool running() const noexcept;
    void requestShutdown() noexcept;

    std::string modeName() const;

private:
    ServerMode mode_;
    bool running_{true};
};
