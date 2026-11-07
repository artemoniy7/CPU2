#include "ConfigManager.h"
#include "ServerShell.h"
#include "ServerState.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cpu16-server-tests-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void writeText(const std::filesystem::path& path, const std::string& content) {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "Could not create test file: " + path.string());
    output << content;
    output.close();
    require(static_cast<bool>(output), "Could not finish test file: " + path.string());
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not read test file: " + path.string());
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

void testConfigurationRoundTrip() {
    TemporaryDirectory temporary;
    const auto configPath = temporary.path() / "config" / "server.ini";
    const auto logPath = temporary.path() / "logs" / "events.log";

    ServerState state(ServerMode::Standalone);
    std::string error;
    require(state.setLogFile(logPath.string(), error), "Could not initialize log: " + error);
    require(state.setHostname("node-01"), "Valid hostname was rejected.");
    state.addLog("round-trip marker");

    ConfigManager config(configPath.string());
    require(config.save(state, error), "Configuration save failed: " + error);
    require(state.setHostname("temporary"), "Could not change hostname for test.");
    require(state.setLogFile((temporary.path() / "other.log").string(), error),
            "Could not switch to test log: " + error);

    require(config.load(state, error), "Configuration load failed: " + error);
    require(error.empty(), "Successful configuration load returned a warning.");
    require(state.hostname() == "node-01", "Hostname did not survive save/load.");
    require(state.logFilePath() == logPath.string(), "Log path did not survive save/load.");
    bool foundMarker = false;
    for (const auto& entry : state.log()) {
        if (entry.find("round-trip marker") != std::string::npos) foundMarker = true;
    }
    require(foundMarker, "Existing persistent log history was not restored.");
}

void testMissingConfigurationCreatesDefaultsAndLoadsLog() {
    TemporaryDirectory temporary;
    const auto configPath = temporary.path() / "config" / "server.ini";
    const auto logPath = temporary.path() / "logs" / "events.log";

    ServerState state(ServerMode::Standalone);
    std::string error;
    require(state.setLogFile(logPath.string(), error), "Could not initialize log: " + error);
    state.addLog("previous session marker");

    ConfigManager config(configPath.string());
    require(config.load(state, error), "Missing configuration was not initialized: " + error);
    require(!error.empty() && error.find("missing") != std::string::npos,
            "Missing configuration should report that defaults were created.");
    require(std::filesystem::exists(configPath), "Default configuration file was not created.");
    require(!state.log().empty() &&
            state.log().front().find("previous session marker") != std::string::npos,
            "Existing log history was lost when the configuration file was missing.");
}

void testInvalidConfigurationDoesNotPartiallyApply() {
    TemporaryDirectory temporary;
    const auto configPath = temporary.path() / "server.ini";
    const auto oldLog = temporary.path() / "old.log";
    const auto newLog = temporary.path() / "new.log";

    ServerState state(ServerMode::Standalone);
    std::string error;
    require(state.setLogFile(oldLog.string(), error), "Could not initialize old log: " + error);
    state.addLog("keep this history");
    const std::string originalHostname = state.hostname();

    writeText(configPath,
              "hostname=changed-node\n"
              "log_file=" + newLog.string() + "\n"
              "unknown_key=value\n");

    ConfigManager config(configPath.string());
    require(!config.load(state, error), "Unknown configuration key was accepted.");
    require(state.hostname() == originalHostname, "Invalid config changed the hostname.");
    require(state.logFilePath() == oldLog.string(), "Invalid config changed the active log path.");
    require(!state.log().empty() &&
            state.log().back().find("keep this history") != std::string::npos,
            "Invalid config changed the in-memory log history.");
    require(!std::filesystem::exists(newLog), "Invalid config opened its proposed log file.");
}

void testUnusableLogTargetDoesNotChangeActiveConfiguration() {
    TemporaryDirectory temporary;
    const auto configPath = temporary.path() / "server.ini";
    const auto oldLog = temporary.path() / "old.log";
    const auto blocker = temporary.path() / "not-a-directory";

    writeText(blocker, "this path is a regular file");
    ServerState state(ServerMode::Standalone);
    std::string error;
    require(state.setLogFile(oldLog.string(), error), "Could not initialize old log: " + error);
    state.addLog("active log marker");
    const std::string originalHostname = state.hostname();

    writeText(configPath,
              "hostname=valid-node\n"
              "log_file=" + (blocker / "events.log").string() + "\n");

    ConfigManager config(configPath.string());
    require(!config.load(state, error), "Unusable log target was accepted.");
    require(state.hostname() == originalHostname, "Failed log activation changed hostname.");
    require(state.logFilePath() == oldLog.string(), "Failed log activation changed active path.");
    require(state.loggingAvailable(), "Failed candidate path damaged the current logger.");
    require(!state.log().empty() &&
            state.log().back().find("active log marker") != std::string::npos,
            "Failed log activation discarded the active log history.");
}

void testPersistentLogHistoryAndClear() {
    TemporaryDirectory temporary;
    const auto logPath = temporary.path() / "logs" / "events.log";

    ServerState state(ServerMode::Standalone);
    std::string error;
    require(state.setLogFile(logPath.string(), error), "Could not initialize log: " + error);
    for (int i = 0; i < 105; ++i) state.addLog("event-" + std::to_string(i));
    require(state.log().size() == 100, "In-memory log did not enforce its 100-entry limit.");

    ServerState restored(ServerMode::Standalone);
    require(restored.setLogFile(logPath.string(), error), "Could not restore log: " + error);
    require(restored.log().size() == 100, "Startup did not load the latest 100 log entries.");
    require(restored.log().back().find("event-104") != std::string::npos,
            "The newest persistent event was not restored.");

    require(restored.clearLog(), "Persistent log clear failed.");
    require(restored.log().size() == 1 &&
            restored.log().front().find("Event log cleared.") != std::string::npos,
            "Clear should leave a single audit event.");
    const std::string persisted = readText(logPath);
    require(persisted.find("Event log cleared.") != std::string::npos,
            "Log clear audit event was not persisted.");
    require(persisted.find("event-104") == std::string::npos,
            "Old events remained in the cleared persistent file.");
}

void testShellConfigurationAndLogCommands() {
    TemporaryDirectory temporary;
    const auto configPath = temporary.path() / "config" / "server.ini";
    const auto logPath = temporary.path() / "logs" / "events.log";

    ServerState state(ServerMode::Standalone);
    std::string error;
    require(state.setLogFile(logPath.string(), error), "Could not initialize log: " + error);
    ConfigManager config(configPath.string());
    require(config.save(state, error), "Could not create initial config: " + error);

    std::istringstream commands(
        "HOSTNAME shell-node\n"
        "CONFIG SAVE\n"
        "HOSTNAME unsaved-node\n"
        "CONFIG RELOAD\n"
        "LOG PATH\n"
        "LOG CLEAR\n"
        "SHUTDOWN\n");
    std::ostringstream output;
    auto* oldInput = std::cin.rdbuf(commands.rdbuf());
    auto* oldOutput = std::cout.rdbuf(output.rdbuf());
    try {
        ServerShell shell(state, config);
        shell.run();
    } catch (...) {
        std::cin.rdbuf(oldInput);
        std::cout.rdbuf(oldOutput);
        std::cin.clear();
        throw;
    }
    std::cout.flush();
    std::cin.rdbuf(oldInput);
    std::cout.rdbuf(oldOutput);
    std::cin.clear();

    require(!state.running(), "SHUTDOWN did not stop the shell.");
    require(state.hostname() == "shell-node", "CONFIG RELOAD did not restore the saved hostname.");
    require(output.str().find("Configuration saved.") != std::string::npos,
            "CONFIG SAVE command did not report success.");
    require(output.str().find("Configuration reloaded.") != std::string::npos,
            "CONFIG RELOAD command did not report success.");
    require(output.str().find(logPath.string()) != std::string::npos,
            "LOG PATH did not display the configured log file.");
    require(output.str().find("Event log cleared.") != std::string::npos,
            "LOG CLEAR command did not report success.");

    const std::string persistedConfig = readText(configPath);
    require(persistedConfig.find("hostname=shell-node") != std::string::npos,
            "Shell did not persist the updated hostname.");
    const std::string persistedLog = readText(logPath);
    require(persistedLog.find("Event log cleared.") != std::string::npos,
            "Shell did not persist the log-clear audit event.");
    require(persistedLog.find("Shutdown requested.") != std::string::npos,
            "Shutdown event was not persisted.");
}
}

int main() {
    const std::vector<std::pair<const char*, void (*)()>> tests = {
        {"configuration round trip", testConfigurationRoundTrip},
        {"missing configuration defaults", testMissingConfigurationCreatesDefaultsAndLoadsLog},
        {"invalid configuration is atomic", testInvalidConfigurationDoesNotPartiallyApply},
        {"unusable log target is atomic", testUnusableLogTargetDoesNotChangeActiveConfiguration},
        {"persistent log history and clear", testPersistentLogHistoryAndClear},
        {"shell config and log commands", testShellConfigurationAndLogCommands}
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "[PASS] " << test.first << "\n";
        } catch (const std::exception& exception) {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": " << exception.what() << "\n";
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All server tests passed.\n";
    return 0;
}