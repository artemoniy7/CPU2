#include "ServerState.h"

ServerState::ServerState(ServerMode mode) : mode_(mode) {}

ServerMode ServerState::mode() const noexcept {
    return mode_;
}

bool ServerState::running() const noexcept {
    return running_;
}

void ServerState::requestShutdown() noexcept {
    running_ = false;
}

std::string ServerState::modeName() const {
    return mode_ == ServerMode::Standalone ? "STANDALONE" : "ATTACHED";
}
