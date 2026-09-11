#include <iostream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <stack>
#include <bitset>
#include <cstdint>
#include <string>
#include <array>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cmath>
#include <GLFW/glfw3.h>
#include <SFML/Audio.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace fs = std::filesystem;

// ============================================================
//  UTILITY FUNCTIONS
// ============================================================
string getExecutablePath() {
#ifdef _WIN32
    char buffer[1024];
    GetModuleFileNameA(NULL, buffer, sizeof(buffer));
    fs::path exePath(buffer);
    return exePath.parent_path().string();
#else
    return fs::current_path().string();
#endif
}

string getSoundsPath() {
    fs::path exePath(getExecutablePath());
    fs::path soundsPath = exePath / "sounds";
    return soundsPath.string();
}

// ============================================================
//  1. VIRTUAL DISK (10 MB) with Directory Support
//     + Proper free-space allocator (no more duplication!)
// ============================================================
class VirtualDisk {
public:
    struct FileEntry {
        size_t offset;
        size_t size;
        string path;
        bool isDirectory;
    };

private:
    static const size_t SIZE = 10 * 1024 * 1024;
    static const size_t METADATA_SIZE = 64 * 1024;
    // Usable region for file data: [0, DATA_LIMIT)
    static const size_t DATA_LIMIT = SIZE - METADATA_SIZE;

    vector<uint8_t> data;
    unordered_map<string, FileEntry> fileTable;

    // Free-space allocator.
    // Each range is [start, end) — half-open, so length = end - start.
    struct FreeRange { size_t start; size_t end; };
    vector<FreeRange> freeList;

    string currentDir = "/";
    string diskFileName;

    // ---------- Allocator ----------
    void rebuildFreeList() {
        freeList.clear();
        // Collect all used ranges (only files, not directories).
        vector<pair<size_t, size_t>> used;
        for (const auto& [path, entry] : fileTable) {
            if (!entry.isDirectory && entry.size > 0) {
                used.emplace_back(entry.offset, entry.offset + entry.size);
            }
        }
        sort(used.begin(), used.end());

        size_t cursor = 0;
        for (const auto& [s, e] : used) {
            if (s > cursor) freeList.push_back({cursor, s});
            cursor = max(cursor, e);
        }
        if (cursor < DATA_LIMIT) freeList.push_back({cursor, DATA_LIMIT});
        coalesceFree();
    }

    void coalesceFree() {
        if (freeList.empty()) return;
        sort(freeList.begin(), freeList.end(),
             [](const FreeRange& a, const FreeRange& b){ return a.start < b.start; });
        vector<FreeRange> merged;
        for (const auto& r : freeList) {
            if (!merged.empty() && merged.back().end >= r.start) {
                merged.back().end = max(merged.back().end, r.end);
            } else {
                merged.push_back(r);
            }
        }
        freeList.swap(merged);
    }

    // Allocate `size` bytes. Returns {offset, true} on success,
    // {0, false} if no contiguous block is large enough.
    pair<size_t, bool> allocSpace(size_t size) {
        if (size == 0) return {0, true};
        for (size_t i = 0; i < freeList.size(); ++i) {
            size_t len = freeList[i].end - freeList[i].start;
            if (len >= size) {
                size_t offset = freeList[i].start;
                freeList[i].start += size;
                if (freeList[i].start == freeList[i].end) {
                    freeList.erase(freeList.begin() + i);
                }
                coalesceFree();
                return {offset, true};
            }
        }
        return {0, false};
    }

    void freeSpace(size_t offset, size_t size) {
        if (size == 0) return;
        freeList.push_back({offset, offset + size});
        coalesceFree();
    }

    // ---------- Persistence ----------
    void loadFromDisk() {
        data.assign(SIZE, 0);
        ifstream file(diskFileName, ios::binary);
        if (file) {
            file.read(reinterpret_cast<char*>(data.data()), SIZE);
            file.close();
            const char* manifest = reinterpret_cast<const char*>(data.data() + SIZE - METADATA_SIZE);
            if (string(manifest, 8) == "CPU16FS1") {
                istringstream input(manifest + 8);
                string line;
                while (getline(input, line) && line != "END") {
                    if (line.rfind("FREE ", 0) == 0) {
                        // Optional: persisted free list (we rebuild anyway).
                        continue;
                    }
                    istringstream row(line);
                    FileEntry entry;
                    int directory = 0;
                    if (row >> entry.offset >> entry.size >> directory >> quoted(entry.path)) {
                        entry.isDirectory = directory != 0;
                        fileTable[entry.path] = entry;
                    }
                }
            }
            cout << "Loaded disk from: " << diskFileName << "\n";
        } else {
            cout << "Created new disk: " << diskFileName << "\n";
        }
        rebuildFreeList();
    }

    void saveToDisk() {
        ostringstream manifest;
        manifest << "CPU16FS1\n";
        for (const auto& [path, entry] : fileTable)
            manifest << entry.offset << ' ' << entry.size << ' '
                     << entry.isDirectory << ' ' << quoted(entry.path) << "\n";
        manifest << "END\n";
        const string listing = manifest.str();
        if (listing.size() > METADATA_SIZE) return;

        fill(data.begin() + SIZE - METADATA_SIZE, data.end(), 0);
        copy(listing.begin(), listing.end(), data.begin() + SIZE - METADATA_SIZE);

        ofstream file(diskFileName, ios::binary | ios::trunc);
        if (file) file.write(reinterpret_cast<const char*>(data.data()), SIZE);
    }

    // ---------- Path helpers ----------
    string normalizePath(const string& path) const {
        if (path.empty()) return currentDir;
        if (path[0] == '/') return path;
        if (currentDir == "/") return "/" + path;
        return currentDir + "/" + path;
    }

    string getParentPath(const string& path) const {
        if (path == "/") return "/";
        size_t pos = path.find_last_of('/');
        if (pos == 0 || pos == string::npos) return "/";
        return path.substr(0, pos);
    }

    string getFileName(const string& path) const {
        if (path == "/") return "/";
        size_t pos = path.find_last_of('/');
        if (pos == string::npos) return path;
        return path.substr(pos + 1);
    }

    void createDirectoryEntry(const string& path) {
        FileEntry entry;
        entry.offset = 0;
        entry.size = 0;
        entry.path = path;
        entry.isDirectory = true;
        fileTable[path] = entry;
    }

public:
    VirtualDisk() {
        fs::path exePath(getExecutablePath());
        diskFileName = (exePath / "disc_c.bin").string();
        loadFromDisk();
        if (fileTable.empty()) {
            createDirectoryEntry("/");
            rebuildFreeList();
            saveToDisk();
        }
    }

    ~VirtualDisk() {
        saveToDisk();
    }

    // ---------- Filesystem API ----------
    bool createDirectory(const string& path) {
        string normalized = normalizePath(path);
        if (fileTable.count(normalized)) return false;

        string parent = getParentPath(normalized);
        if (!fileTable.count(parent) || !fileTable[parent].isDirectory) return false;

        createDirectoryEntry(normalized);
        saveToDisk();
        return true;
    }

    bool createFile(const string& name) {
        string normalized = normalizePath(name);
        if (fileTable.count(normalized)) return false;

        string parent = getParentPath(normalized);
        if (!fileTable.count(parent) || !fileTable[parent].isDirectory) return false;

        FileEntry entry;
        entry.offset = 0;
        entry.size = 0;
        entry.path = normalized;
        entry.isDirectory = false;
        fileTable[normalized] = entry;
        saveToDisk();
        return true;
    }

    // Append: extends the file in place if possible, otherwise allocates
    // a new contiguous region and copies the old content into it.
    bool appendToFile(const string& name, const string& content) {
        string normalized = normalizePath(name);
        auto it = fileTable.find(normalized);
        if (it == fileTable.end() || it->second.isDirectory) return false;

        FileEntry& entry = it->second;
        if (content.empty()) return true;

        // Try to extend in place: is [entry.offset+entry.size, ...+content) free?
        size_t tail = entry.offset + entry.size;
        for (auto& r : freeList) {
            if (r.start <= tail && tail + content.size() <= r.end) {
                // Consume from this range.
                if (r.start == tail) {
                    r.start += content.size();
                } else {
                    // tail is in the middle of r — shouldn't happen if ranges
                    // are correctly maintained, but be safe:
                    size_t oldEnd = r.end;
                    r.end = tail;
                    freeList.push_back({tail + content.size(), oldEnd});
                    coalesceFree();
                }
                copy(content.begin(), content.end(), data.begin() + tail);
                entry.size += content.size();
                saveToDisk();
                return true;
            }
        }
        // Fallback: allocate new region and copy.
        return writeFile(name, readFile(name) + content);
    }

    string readFile(const string& name) {
        string normalized = normalizePath(name);
        auto it = fileTable.find(normalized);
        if (it == fileTable.end() || it->second.isDirectory) return "";

        size_t offset = it->second.offset;
        size_t size = it->second.size;
        if (offset + size > SIZE) return "";
        return string(data.begin() + offset, data.begin() + offset + size);
    }

    // The fix: free the old region BEFORE allocating a new one.
    bool writeFile(const string& name, const string& content) {
        string normalized = normalizePath(name);
        auto it = fileTable.find(normalized);
        if (it == fileTable.end() || it->second.isDirectory) return false;

        FileEntry& entry = it->second;

        // 1. Release old space.
        if (entry.size > 0) {
            freeSpace(entry.offset, entry.size);
        }
        entry.offset = 0;
        entry.size = 0;

        // 2. Allocate new space (empty file is allowed).
        if (!content.empty()) {
            auto [offset, ok] = allocSpace(content.size());
            if (!ok) {
                // Could not fit — mark file as empty and report failure.
                saveToDisk();
                return false;
            }
            entry.offset = offset;
            entry.size = content.size();
            copy(content.begin(), content.end(), data.begin() + offset);
        }
        saveToDisk();
        return true;
    }

    // Delete a file (not a directory).
    bool deleteFile(const string& name) {
        string normalized = normalizePath(name);
        auto it = fileTable.find(normalized);
        if (it == fileTable.end() || it->second.isDirectory) return false;

        if (it->second.size > 0) {
            freeSpace(it->second.offset, it->second.size);
        }
        fileTable.erase(it);
        saveToDisk();
        return true;
    }

    // Delete a directory and everything under it.
    bool deleteDirectory(const string& path) {
        string normalized = normalizePath(path);
        if (normalized == "/") return false;  // never delete root
        auto it = fileTable.find(normalized);
        if (it == fileTable.end() || !it->second.isDirectory) return false;

        // Collect all descendants (including the dir itself).
        vector<string> toErase;
        toErase.push_back(normalized);
        for (const auto& [p, e] : fileTable) {
            if (p.size() > normalized.size() &&
                p.compare(0, normalized.size(), normalized) == 0 &&
                p[normalized.size()] == '/') {
                toErase.push_back(p);
            }
        }
        for (const auto& p : toErase) {
            auto f = fileTable.find(p);
            if (f != fileTable.end() && !f->second.isDirectory && f->second.size > 0) {
                freeSpace(f->second.offset, f->second.size);
            }
            fileTable.erase(f);
        }
        saveToDisk();
        return true;
    }

    string listFiles() {
        ostringstream output;
        output << "\n--- Files on disk (" << SIZE/1024/1024 << " MB) ---\n";
        output << "Current directory: " << currentDir << "\n\n";

        for (auto& [path, entry] : fileTable) {
            if (path == "/" || path == currentDir) continue;
            string parent = getParentPath(path);
            if (parent != currentDir) continue;

            string name = getFileName(path);
            if (entry.isDirectory) {
                output << "  [DIR]  " << name << "/\n";
            } else {
                output << "  [FILE] " << name << " (" << entry.size << " bytes)\n";
            }
        }
        output << "------------------------------------\n";
        return output.str();
    }

    bool changeDirectory(const string& path) {
        string normalized = normalizePath(path);
        if (!fileTable.count(normalized) || !fileTable[normalized].isDirectory) return false;
        currentDir = normalized;
        return true;
    }

    string getCurrentDirectory() const { return currentDir; }

    bool fileExists(const string& name) {
        string normalized = normalizePath(name);
        return fileTable.count(normalized) > 0;
    }

    size_t getFileSize(const string& name) {
        string normalized = normalizePath(name);
        auto it = fileTable.find(normalized);
        if (it == fileTable.end() || it->second.isDirectory) return 0;
        return it->second.size;
    }

    vector<FileEntry> getCurrentEntries() const {
        vector<FileEntry> entries;
        for (const auto& [path, entry] : fileTable) {
            if (path != "/" && getParentPath(path) == currentDir) entries.push_back(entry);
        }
        sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            return a.path < b.path;
        });
        return entries;
    }

    string getName(const string& path) const { return getFileName(path); }
    bool isDirectory(const string& path) const {
        auto it = fileTable.find(normalizePath(path));
        return it != fileTable.end() && it->second.isDirectory;
    }

    void reset() {
        data.assign(SIZE, 0);
        fileTable.clear();
        freeList.clear();
        createDirectoryEntry("/");
        currentDir = "/";
        rebuildFreeList();
        saveToDisk();
    }

    uint8_t readByte(size_t addr) const {
        if (addr < SIZE) return data[addr];
        return 0;
    }

    void writeByte(size_t addr, uint8_t val) {
        if (addr < SIZE) {
            data[addr] = val;
            saveToDisk();
        }
    }

    void* getData() { return data.data(); }
    size_t getSize() const { return SIZE; }
};

// ============================================================
//  2. AUDIO MANAGER - Исправленная версия для SFML 3.x
// ============================================================
class AudioManager {
private:
    sf::SoundBuffer powerOnBuffer;
    sf::SoundBuffer powerOffBuffer;
    sf::Sound powerOnSound;
    sf::Sound powerOffSound;
    bool initialized = true;
    bool powerOnPlaying = false;
    static constexpr float powerOnLoopOffsetSeconds = 20.0f;

    void generateBeep(sf::SoundBuffer& buffer, float frequency, float duration) {
        const int sampleRate = 44100;
        const int numSamples = static_cast<int>(sampleRate * duration);
        vector<int16_t> samples(numSamples);

        for (int i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float value = sinf(2.0f * 3.14159f * frequency * t) * 0.5f;
            float decay = 1.0f - (t / duration);
            value *= decay;
            samples[i] = static_cast<int16_t>(value * 32767.0f);
        }
        buffer.loadFromSamples(samples.data(), numSamples, 1, sampleRate, {});
    }

public:
    AudioManager() : powerOnSound(powerOnBuffer), powerOffSound(powerOffBuffer) {
        string soundsPath = getSoundsPath();
        string soundPath = soundsPath + "/power_on.ogg";
        string soundOffPath = soundsPath + "/power_off.ogg";

        cout << "Looking for power_on.ogg at: " << soundPath << "\n";
        cout << "Looking for power_off.ogg at: " << soundOffPath << "\n";

        bool loadedOn = false;
        bool loadedOff = false;

        if (powerOnBuffer.loadFromFile(soundPath)) {
            cout << "Loaded power_on.ogg successfully\n";
            loadedOn = true;
        } else {
            string wavPath = soundsPath + "/power_on.wav";
            if (powerOnBuffer.loadFromFile(wavPath)) {
                cout << "Loaded power_on.wav successfully\n";
                loadedOn = true;
            }
        }
        if (!loadedOn) {
            cout << "Warning: Could not load power_on sound, generating beep\n";
            generateBeep(powerOnBuffer, 440.0f, 0.3f);
        }

        if (powerOffBuffer.loadFromFile(soundOffPath)) {
            cout << "Loaded power_off.ogg successfully\n";
            loadedOff = true;
        } else {
            string wavPath = soundsPath + "/power_off.wav";
            if (powerOffBuffer.loadFromFile(wavPath)) {
                cout << "Loaded power_off.wav successfully\n";
                loadedOff = true;
            }
        }
        if (!loadedOff) {
            cout << "Warning: Could not load power_off sound, generating beep\n";
            generateBeep(powerOffBuffer, 220.0f, 0.2f);
        }

        powerOnSound.setBuffer(powerOnBuffer);
        powerOffSound.setBuffer(powerOffBuffer);
        initialized = true;
        cout << "AudioManager initialized successfully\n";
    }

    ~AudioManager() {
        powerOnSound.stop();
        powerOffSound.stop();
    }

    void playPowerOn() {
        if (!initialized) return;
        powerOnSound.stop();
        powerOnSound.setLooping(false);
        powerOnSound.setPlayingOffset(sf::seconds(0.0f));
        powerOnSound.play();
        powerOnPlaying = true;
    }

    void playPowerOff() {
        if (!initialized) return;
        powerOnPlaying = false;
        powerOnSound.stop();
        powerOnSound.setLooping(false);
        powerOffSound.stop();
        powerOffSound.play();
    }

    void update() {
        if (!initialized || !powerOnPlaying) return;
        if (powerOnSound.getStatus() == sf::SoundSource::Status::Stopped) {
            powerOnSound.setPlayingOffset(sf::seconds(powerOnLoopOffsetSeconds));
            powerOnSound.play();
        }
    }

    void stopPowerOn() {
        if (!initialized) return;
        powerOnPlaying = false;
        powerOnSound.stop();
        powerOnSound.setLooping(false);
    }

    bool isPowerOnPlaying() const { return powerOnPlaying; }
};

// ============================================================
//  3. 16-BIT CPU WITH FLAGS AND CACHE
// ============================================================
class CPU {
private:
    uint16_t AX=0, BX=0, CX=0, DX=0;
    uint16_t SP=0xFFFE, BP=0, SI=0, DI=0;
    uint16_t CS=0, DS=0, SS=0, ES=0;
    uint16_t IP = 0;

    bool ZF=false, CF=false, SF=false, OF=false, DF=false;
    stack<uint16_t> callStack;

    struct CacheLine { bool valid=false; uint32_t tag=0; uint8_t data[64]; };
    static const int CACHE_SIZE = 8;
    CacheLine cache[CACHE_SIZE];

    VirtualDisk* disk;
    vector<uint8_t> ram = vector<uint8_t>(1024 * 1024, 0);
    bool debugMode = false;
    bool powered = false;
    string loadedSource;
    vector<string> programLines;
    unordered_map<string, int> programLabels;
    string terminalInput;
    bool waitingForTerminalInput = false;
    uint16_t waitingInputPort = 0;
    uint16_t lastKeyScanCode = 0;
    chrono::steady_clock::time_point startedAt = chrono::steady_clock::now();

public:
    CPU(VirtualDisk* d) : disk(d) {}

    void setDebug(bool on) { debugMode = on; }
    void setPowered(bool on) { powered = on; }
    bool isPowered() const { return powered; }
    void setTerminalInput(const string& input) { terminalInput = input; waitingForTerminalInput = false; }
    bool isWaitingForTerminalInput() const { return waitingForTerminalInput; }
    uint16_t getWaitingInputPort() const { return waitingInputPort; }
    void setLastKeyScanCode(uint16_t code) { lastKeyScanCode = code; }
    void stopProgram() { loadedSource.clear(); programLines.clear(); programLabels.clear(); IP = 0; waitingForTerminalInput = false; }
    bool hasProgram() const { return !programLines.empty() && IP < programLines.size(); }

    void printRegs() {
        if (!powered) return;
        cout << "AX=0x" << hex << setw(4) << setfill('0') << AX
             << " BX=0x" << setw(4) << BX
             << " CX=0x" << setw(4) << CX
             << " DX=0x" << setw(4) << DX << "\n";
        cout << "SP=0x" << setw(4) << SP << " BP=0x" << setw(4) << BP
             << " SI=0x" << setw(4) << SI << " DI=0x" << setw(4) << DI << "\n";
    }

    void reset() {
        AX=BX=CX=DX=SP=BP=SI=DI=CS=DS=SS=ES=IP=0;
        ZF=CF=SF=OF=DF=false;
        while(!callStack.empty()) callStack.pop();
        for(auto& line : cache) line.valid = false;
        fill(ram.begin(), ram.end(), 0);
        stopProgram();
        startedAt = chrono::steady_clock::now();
    }

    uint8_t readByte(uint32_t addr) {
        if (!powered) return 0;
        uint32_t tag = addr / 64, offset = addr % 64;
        for (int i = 0; i < CACHE_SIZE; i++)
            if (cache[i].valid && cache[i].tag == tag)
                return cache[i].data[offset];

        int idx = addr % CACHE_SIZE;
        cache[idx].valid = true;
        cache[idx].tag = tag;
        for (size_t i = 0; i < 64; i++)
            cache[idx].data[i] = (tag * 64 + i < ram.size()) ? ram[tag * 64 + i] : 0;
        return cache[idx].data[offset];
    }

    uint16_t readWord(uint32_t addr) {
        if (!powered) return 0;
        return readByte(addr) | (readByte(addr+1) << 8);
    }

    void writeByte(uint32_t addr, uint8_t val) {
        if (!powered) return;
        uint32_t tag = addr / 64, offset = addr % 64;
        for (int i = 0; i < CACHE_SIZE; i++)
            if (cache[i].valid && cache[i].tag == tag) {
                cache[i].data[offset] = val;
                if (addr < ram.size()) ram[addr] = val;
                return;
            }
        if (addr < ram.size()) ram[addr] = val;
    }

    void writeWord(uint32_t addr, uint16_t val) {
        if (!powered) return;
        writeByte(addr, val & 0xFF);
        writeByte(addr+1, (val >> 8) & 0xFF);
    }

    void setFlagsForResult(uint16_t result, uint16_t op1, uint16_t op2, bool isAdd) {
        ZF = (result == 0);
        SF = (result & 0x8000) != 0;
        if (isAdd) {
            OF = ((op1 & 0x8000) == (op2 & 0x8000)) &&
                 ((result & 0x8000) != (op1 & 0x8000));
            CF = (result < op1) || (result < op2);
        } else {
            OF = ((op1 & 0x8000) != (op2 & 0x8000)) &&
                 ((result & 0x8000) != (op1 & 0x8000));
            CF = (op1 < op2);
        }
    }

    string executeProgram(const string& code) {
        ostringstream output;
        if (!powered) {
            output << "CPU is powered off\n";
            return output.str();
        }

        if (loadedSource != code) {
            loadedSource = code;
            programLines.clear();
            programLabels.clear();
            istringstream iss(code);
            string line;
            while (getline(iss, line)) {
                size_t comment = line.find(';');
                if (comment != string::npos) line = line.substr(0, comment);
                size_t first = line.find_first_not_of(" \t");
                if (first == string::npos) continue;
                line.erase(0, first);
                size_t last = line.find_last_not_of(" \t");
                line.erase(last + 1);
                if (line.back() == ':') {
                    programLabels[line.substr(0, line.size()-1)] = programLines.size();
                    continue;
                }
                programLines.push_back(line);
            }
            IP = 0;
        }
        if (IP < programLines.size()) {
            if (!powered) {
                output << "CPU powered off during execution\n";
                return output.str();
            }
            string instr = programLines[IP];
            IP++;
            istringstream cmdStream(instr);
            string opcode;
            cmdStream >> opcode;

            if (opcode == "MOV") {
                string dest, src; cmdStream >> dest >> src;
                uint16_t val = getValue(src);
                setRegister(dest, val);
            }
            else if (opcode == "XCHG") {
                string r1, r2; cmdStream >> r1 >> r2;
                uint16_t val1 = getRegister(r1);
                uint16_t val2 = getRegister(r2);
                setRegister(r1, val2);
                setRegister(r2, val1);
            }
            else if (opcode == "PUSH") {
                string src; cmdStream >> src;
                SP -= 2;
                writeWord(SP, getValue(src));
            }
            else if (opcode == "POP") {
                string dest; cmdStream >> dest;
                uint16_t val = readWord(SP);
                SP += 2;
                setRegister(dest, val);
            }
            else if (opcode == "ADD" || opcode == "SUB" || opcode == "CMP") {
                string dest, src; cmdStream >> dest >> src;
                uint16_t val1 = getRegister(dest);
                uint16_t val2 = getValue(src);
                uint16_t result = (opcode == "ADD") ? val1 + val2 :
                                 (opcode == "SUB") ? val1 - val2 : val1 - val2;
                if (opcode != "CMP") setRegister(dest, result);
                setFlagsForResult(result, val1, val2, opcode == "ADD");
            }
            else if (opcode == "INC") {
                string dest; cmdStream >> dest;
                uint16_t val = getRegister(dest) + 1;
                setRegister(dest, val);
                ZF = (val == 0); SF = (val & 0x8000) != 0;
            }
            else if (opcode == "DEC") {
                string dest; cmdStream >> dest;
                uint16_t val = getRegister(dest) - 1;
                setRegister(dest, val);
                ZF = (val == 0); SF = (val & 0x8000) != 0;
            }
            else if (opcode == "NEG") {
                string dest; cmdStream >> dest;
                uint16_t val = -getRegister(dest);
                setRegister(dest, val);
                ZF = (val == 0); SF = (val & 0x8000) != 0; CF = (val != 0);
            }
            else if (opcode == "MUL" || opcode == "IMUL") {
                string src; cmdStream >> src;
                uint16_t val = getValue(src);
                uint32_t result = AX * val;
                AX = result & 0xFFFF;
                DX = (result >> 16) & 0xFFFF;
                ZF = (AX == 0); SF = (AX & 0x8000) != 0; CF = (DX != 0);
            }
            else if (opcode == "DIV" || opcode == "IDIV") {
                string src; cmdStream >> src;
                uint16_t divisor = getValue(src);
                if (divisor == 0) { output << "Division by zero!\n"; return output.str(); }
                uint32_t dividend = (DX << 16) | AX;
                AX = dividend / divisor;
                DX = dividend % divisor;
            }
            else if (opcode == "LOAD") {
                string dest, addr; cmdStream >> dest >> addr;
                uint16_t val = readWord(getValue(addr));
                setRegister(dest, val);
            }
            else if (opcode == "STORE") {
                string src, addr; cmdStream >> src >> addr;
                writeWord(getValue(addr), getRegister(src));
            }
            else if (opcode == "IN") {
                string dest, port; cmdStream >> dest >> port;
                uint16_t portNumber = getValue(port), val = 0;
                if ((portNumber == 0x00 || portNumber == 0x02) && terminalInput.empty()) {
                    // Re-run this IN instruction after the terminal supplies a value.
                    --IP;
                    waitingForTerminalInput = true;
                    waitingInputPort = portNumber;
                    output << "Waiting for input on port 0x" << hex << portNumber << "...\n";
                    return output.str();
                }
                if (portNumber == 0x00) val = parseNumber(terminalInput);
                else if (portNumber == 0x02) val = static_cast<uint8_t>(terminalInput[0]);
                else if (portNumber == 0x10) val = static_cast<uint16_t>(chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startedAt).count());
                else if (portNumber == 0x20) val = lastKeyScanCode;
                else if (portNumber == 0x30) val = 0;
                output << "IN 0x" << hex << portNumber << " -> 0x" << val << "\n";
                setRegister(dest, val);
                if (portNumber == 0x00 || portNumber == 0x02) terminalInput.clear();
            }
            else if (opcode == "OUT") {
                string port, src; cmdStream >> port >> src;
                uint16_t portNumber = getValue(port), val = getValue(src);
                if (portNumber == 0x01) output << dec << val << "\n";
                else if (portNumber == 0x03 || portNumber == 0xE9) output << static_cast<char>(val & 0xFF);
                else output << "OUT 0x" << hex << portNumber << " = 0x" << val << " (" << dec << val << ")\n";
            }
            else if (opcode == "PEEK") {
                string addr; cmdStream >> addr;
                uint16_t val = readByte(getValue(addr));
                output << "0x" << hex << setw(4) << setfill('0') << getValue(addr)
                       << " = 0x" << setw(2) << (int)val << "\n";
            }
            else if (opcode == "POKE") {
                string addr, val; cmdStream >> addr >> val;
                writeByte(getValue(addr), getValue(val) & 0xFF);
            }
            else if (opcode == "DUMP") {
                string addr, len; cmdStream >> addr >> len;
                uint32_t start = getValue(addr);
                uint32_t length = getValue(len);
                for (uint32_t i = 0; i < length; i += 16) {
                    output << "0x" << hex << setw(6) << setfill('0') << (start+i) << ": ";
                    for (uint32_t j = 0; j < 16 && i+j < length; j++)
                        output << setw(2) << (int)readByte(start+i+j) << " ";
                    output << "\n";
                }
            }
            else if (opcode == "PRINT") {
                // Читаем всё, что осталось после опкода PRINT, как одну строку.
                string rest;
                getline(cmdStream, rest);

                // Убираем ведущие пробелы
                size_t start = rest.find_first_not_of(" \t");
                if (start == string::npos) {
                    output << "\n";
                } else {
                    rest = rest.substr(start);

                    // Если начинается с кавычки — это строковый литерал.
                    if (!rest.empty() && rest[0] == '"') {
                        // Ищем закрывающую кавычку
                        size_t endQuote = rest.find('"', 1);
                        if (endQuote != string::npos) {
                            output << rest.substr(1, endQuote - 1) << "\n";
                        } else {
                            // Кавычка не закрыта — выводим как есть
                            output << rest.substr(1) << "\n";
                        }
                    } else {
                        // Иначе — число или регистр
                        uint16_t val = getValue(rest);
                        output << "0x" << hex << val << " (" << dec << val << ")\n";
                    }
                }
            }
            else if (opcode == "CLC") { CF = false; }
            else if (opcode == "STC") { CF = true; }
            else if (opcode == "CMC") { CF = !CF; }
            else if (opcode == "CLD") { DF = false; }
            else if (opcode == "STD") { DF = true; }
            else if (opcode == "NOP") { }
            else if (opcode == "HLT") { IP = programLines.size(); output << "Program terminated\n"; return output.str(); }
            else if (opcode == "JMP") {
                string label; cmdStream >> label;
                if (programLabels.count(label)) { IP = programLabels[label]; }
                else { output << "Label " << label << " not found\n"; return output.str(); }
            }
            else if (opcode == "JE" || opcode == "JZ") {
                string label; cmdStream >> label;
                if (ZF && programLabels.count(label)) { IP = programLabels[label]; }
            }
            else if (opcode == "JNE" || opcode == "JNZ") {
                string label; cmdStream >> label;
                if (!ZF && programLabels.count(label)) { IP = programLabels[label]; }
            }
            else if (opcode == "JG") {
                string label; cmdStream >> label;
                if (!ZF && !SF && !OF && programLabels.count(label)) { IP = programLabels[label]; }
            }
            else if (opcode == "JL") {
                string label; cmdStream >> label;
                if (SF != OF && programLabels.count(label)) { IP = programLabels[label]; }
            }
            else if (opcode == "CALL") {
                string label; cmdStream >> label;
                if (programLabels.count(label)) {
                    callStack.push(IP);
                    IP = programLabels[label];
                } else { output << "Label " << label << " not found\n"; return output.str(); }
            }
            else if (opcode == "RET") {
                if (!callStack.empty()) { IP = callStack.top(); callStack.pop(); }
                else { output << "RET without CALL\n"; return output.str(); }
            }
            else if (opcode == "LOOP") {
                string label; cmdStream >> label;
                CX--;
                if (CX != 0 && programLabels.count(label)) { IP = programLabels[label]; }
            }
            else {
                output << "Unknown instruction: " << opcode << "\n";
                return output.str();
            }
        }
        return output.str();
    }

private:
    static uint16_t parseNumber(const string& value) {
        if (value.empty()) return 0;
        size_t start = 0; bool negative = false;
        if (value[0] == '-') { negative = true; start = 1; }
        uint32_t result = 0; int base = 10;
        if (value.size() > start + 2 && value[start] == '0' && (value[start + 1] == 'x' || value[start + 1] == 'X')) { base = 16; start += 2; }
        for (; start < value.size(); ++start) { char c = value[start]; int digit = isdigit((unsigned char)c) ? c - '0' : (isxdigit((unsigned char)c) ? toupper((unsigned char)c) - 'A' + 10 : -1); if (digit < 0 || digit >= base) break; result = result * base + digit; }
        return static_cast<uint16_t>(negative ? -static_cast<int32_t>(result) : result);
    }

    uint16_t getValue(const string& arg) {
        if (arg.empty()) return 0;
        if (arg[0] == 'R' && arg.size()>1 && isdigit(arg[1])) {
            int num = arg[1]-'0';
            if (num >= 0 && num <= 7) {
                uint16_t regs[] = {AX, BX, CX, DX, SP, BP, SI, DI};
                return regs[num];
            }
        }
        if (arg == "AX" || arg == "BX" || arg == "CX" || arg == "DX" ||
            arg == "SP" || arg == "BP" || arg == "SI" || arg == "DI") {
            return getRegister(arg);
        }
        if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) {
            return parseNumber(arg);
        }
        return parseNumber(arg);
    }

    uint16_t getRegister(const string& reg) {
        if (reg == "AX") return AX;
        if (reg == "BX") return BX;
        if (reg == "CX") return CX;
        if (reg == "DX") return DX;
        if (reg == "SP") return SP;
        if (reg == "BP") return BP;
        if (reg == "SI") return SI;
        if (reg == "DI") return DI;
        return 0;
    }

    void setRegister(const string& reg, uint16_t val) {
        if (reg == "AX") { AX = val; return; } if (reg == "BX") { BX = val; return; }
        if (reg == "CX") { CX = val; return; } if (reg == "DX") { DX = val; return; }
        if (reg == "SP") { SP = val; return; } if (reg == "BP") { BP = val; return; }
        if (reg == "SI") { SI = val; return; } if (reg == "DI") { DI = val; return; }
    }
};

// ============================================================
//  4. ASM16 COMPILER: source .asm -> portable .exe container
// ============================================================
class Asm16Compiler {
public:
    static constexpr const char* MAGIC = "ASM16EXE1\n";
    static bool compile(const string& sourcePath, const string& source, const string& parameters,
                        string& executable, string& error) {
        if (sourcePath.empty() || fs::path(sourcePath).extension() != ".asm") {
            error = "Input path must name an .asm source file"; return false;
        }
        if (source.empty()) { error = "Source file is empty"; return false; }
        // Parameters are recorded with the executable so the invocation is reproducible.
        executable = string(MAGIC) + "; asm16 " + sourcePath + " " + parameters + "\n" + source;
        error.clear();
        return true;
    }
    static bool readExecutable(const string& executable, string& source) {
        const string magic(MAGIC);
        if (executable.rfind(magic, 0) != 0) return false;
        source = executable.substr(magic.size());
        return true;
    }
};

// ============================================================
//  4. FULL-SCREEN GLFW TERMINAL INTERFACE
// ============================================================
class TerminalInterface {
private:
    VirtualDisk disk;
    CPU cpu;
    AudioManager audio;
    GLFWwindow* window = nullptr;

    string editor = "";
    string editorFileName;
    string outputBuffer = "";
    string statusBar = "OFFLINE";
    string currentDir = "/";
    string activeExecutable;
    enum class Prompt { None, FileName, DirectoryName, PortInput, CompilerCommand };
    Prompt prompt = Prompt::None;
    string promptText;
    string promptValue;

    bool showHelp = false;
    bool cursorVisible = true;
    double lastBlink = 0.0;
    bool isPowered = false;
    float powerAnimation = 0.0f;
    bool animating = false;
    float animationSpeed = 0.015f;
    bool booting = false;
    float bootProgress = 0.0f;
    vector<string> bootMessages;
    int bootMessageIndex = 0;
    float bootMessageTimer = 0.0f;
    enum class Screen { Menu, Browser, Editor };
    Screen screen = Screen::Menu;
    int menuSelection = 0;
    int fileSelection = 0;
    bool showEditor = false;
    bool powerOnSoundPlayed = false;
    bool powerOffSoundPlayed = false;

    int cursorLine = 0;
    int cursorCol = 0;
    vector<string> editorLines;
    int editorScroll = 0;

    double lastUpdateTime = 0.0;

    void appendOutput(const string& message) {
        if (message.empty()) return;
        outputBuffer += message;
        if (outputBuffer.size() > 2000) {
            size_t pos = outputBuffer.find('\n', outputBuffer.size() - 1000);
            if (pos != string::npos) outputBuffer.erase(0, pos + 1);
        }
    }

    void togglePower() {
        if (animating) return;
        animating = true;
        if (isPowered) {
            booting = false;
            statusBar = "SHUTTING DOWN";
            powerOffSoundPlayed = false;
            audio.stopPowerOn();
        } else {
            booting = true;
            bootProgress = 0.0f;
            bootMessageIndex = 0;
            bootMessageTimer = 0.0f;
            bootMessages = {
                "Initializing CPU...",
                "Loading microcode...",
                "Testing RAM...",
                "Initializing disk controller...",
                "Mounting virtual disk...",
                "Loading system files...",
                "Starting terminal...",
                "System ready."
            };
            statusBar = "BOOTING";
            showEditor = false;
            powerOnSoundPlayed = false;
        }
    }

    void completePowerToggle() {
        isPowered = !isPowered;
        cpu.setPowered(isPowered);
        if (isPowered) {
            statusBar = "ONLINE";
            appendOutput("System ready. Type program above and press F5 to run.\n");
            screen = Screen::Menu;
            showEditor = false;
            if (editor.empty()) {
                editor = "; Welcome to CPU-16\n";
                editor += "; Write your program here\n";
                editor += "MOV AX 0x002A\n";
                editor += "MOV BX 0x0008\n";
                editor += "ADD AX BX\n";
                editor += "OUT 1 AX\n";
                editor += "HLT\n";
            }
            updateEditorLines();
        } else {
            statusBar = "OFFLINE";
            showEditor = false;
            outputBuffer.clear();
            audio.stopPowerOn();
        }
        animating = false;
        booting = false;
    }

    void completeBoot() {
        statusBar = "ONLINE";
        appendOutput("System ready. Type program above and press F5 to run.\n");
        screen = Screen::Menu;
        showEditor = false;
        if (editor.empty()) {
            editor = "; Welcome to CPU-16\n";
            editor += "; Write your program here\n";
            editor += "MOV AX 0x002A\n";
            editor += "MOV BX 0x0008\n";
            editor += "ADD AX BX\n";
            editor += "OUT 1 AX\n";
            editor += "HLT\n";
        }
        updateEditorLines();
        booting = false;
    }

    void runProgram() {
        if (!isPowered) {
            appendOutput("ERROR: System is powered off\n");
            return;
        }
        if (editor.empty()) {
            appendOutput("ERROR: No program to run\n");
            return;
        }
        statusBar = "EXECUTING";
        string result = cpu.executeProgram(editor);
        appendOutput("\n=== PROGRAM OUTPUT ===\n");
        appendOutput(result);
        appendOutput("=== END OF OUTPUT ===\n");
        statusBar = "ONLINE";
    }

    void resetMachine() {
        if (!isPowered) {
            appendOutput("ERROR: System is powered off\n");
            return;
        }
        disk.reset();
        cpu.reset();
        appendOutput("System reset complete\n");
    }

    void updateEditorLines() {
        editorLines.clear();
        istringstream iss(editor);
        string line;
        while (getline(iss, line)) editorLines.push_back(line);
        if (editorLines.empty()) editorLines.push_back("");
        cursorLine = min(cursorLine, (int)editorLines.size() - 1);
        cursorLine = max(0, cursorLine);
        cursorCol = min(cursorCol, (int)editorLines[cursorLine].length());
        cursorCol = max(0, cursorCol);
    }

    void insertChar(char c) {
        if (!isPowered || !showEditor) return;
        if (c == '\n') {
            string currentLine = editorLines[cursorLine];
            string before = currentLine.substr(0, cursorCol);
            string after = currentLine.substr(cursorCol);
            editorLines[cursorLine] = before;
            editorLines.insert(editorLines.begin() + cursorLine + 1, after);
            cursorLine++;
            cursorCol = 0;
        } else {
            string& line = editorLines[cursorLine];
            line.insert(cursorCol, 1, c);
            cursorCol++;
        }
        rebuildEditor();
    }

    void deleteChar() {
        if (!isPowered || !showEditor) return;
        if (cursorCol > 0) {
            string& line = editorLines[cursorLine];
            line.erase(cursorCol - 1, 1);
            cursorCol--;
        } else if (cursorLine > 0) {
            string prevLine = editorLines[cursorLine - 1];
            string currentLine = editorLines[cursorLine];
            editorLines[cursorLine - 1] = prevLine + currentLine;
            editorLines.erase(editorLines.begin() + cursorLine);
            cursorLine--;
            cursorCol = prevLine.length();
        }
        rebuildEditor();
    }

    void rebuildEditor() {
        editor.clear();
        for (const string& line : editorLines) editor += line + "\n";
        if (!editor.empty() && editor.back() == '\n') editor.pop_back();
    }

    void moveCursorUp() {
        if (cursorLine > 0) {
            cursorLine--;
            cursorCol = min(cursorCol, (int)editorLines[cursorLine].length());
        }
    }

    void moveCursorDown() {
        if (cursorLine < (int)editorLines.size() - 1) {
            cursorLine++;
            cursorCol = min(cursorCol, (int)editorLines[cursorLine].length());
        }
    }

    void moveCursorLeft() {
        if (cursorCol > 0) cursorCol--;
        else if (cursorLine > 0) {
            cursorLine--;
            cursorCol = editorLines[cursorLine].length();
        }
    }

    void moveCursorRight() {
        if (cursorCol < (int)editorLines[cursorLine].length()) cursorCol++;
        else if (cursorLine < (int)editorLines.size() - 1) {
            cursorLine++;
            cursorCol = 0;
        }
    }

    void beginPrompt(Prompt kind, const string& text) {
        prompt = kind; promptText = text; promptValue.clear();
    }

    void finishPrompt() {
        string value = promptValue;
        Prompt kind = prompt;
        prompt = Prompt::None;
        if (value.empty()) { appendOutput("Cancelled: name/input is empty\n"); return; }
        if (kind == Prompt::FileName) {
            if (value.find('.') == string::npos) value += ".asm";
            if (!disk.createFile(value)) { appendOutput("ERROR: File already exists or parent folder is missing\n"); return; }
            editorFileName = value; editor = "; " + value + "\n; CPU-16 program\n\nHLT";
            cursorLine = cursorCol = 0; updateEditorLines(); screen = Screen::Editor; showEditor = true;
            outputBuffer.clear();
            appendOutput("Created and opened: " + value + "\n");
        } else if (kind == Prompt::DirectoryName) {
            if (disk.createDirectory(value)) appendOutput("Created directory: " + value + "\n");
            else appendOutput("ERROR: Directory already exists or parent folder is missing\n");
        } else if (kind == Prompt::PortInput) {
            const bool resumeExecution = cpu.isWaitingForTerminalInput();
            cpu.setTerminalInput(value); appendOutput("Port 0x00/0x02 input set: " + value + "\n");
            if (resumeExecution && !activeExecutable.empty()) runExecutable(activeExecutable);
        } else if (kind == Prompt::CompilerCommand) {
            istringstream command(value);
            string verb, sourcePath, option, outputName;
            command >> verb >> sourcePath;
            if (verb != "compile" || sourcePath.empty()) {
                appendOutput("ASM16 usage: compile <source.asm> [-o <program.exe>]\n"); return;
            }
            if (command >> option) {
                if (option != "-o" || !(command >> outputName)) {
                    appendOutput("ASM16 usage: compile <source.asm> [-o <program.exe>]\n"); return;
                }
            }
            if (outputName.empty()) { fs::path outputPath(sourcePath); outputPath.replace_extension(".exe"); outputName = outputPath.string(); }
            string executable, error;
            if (!Asm16Compiler::compile(sourcePath, disk.readFile(sourcePath), "-o " + outputName, executable, error)) {
                appendOutput("ASM16 ERROR: " + error + "\n"); return;
            }
            if (!disk.fileExists(outputName) && !disk.createFile(outputName)) {
                appendOutput("ASM16 ERROR: Cannot create " + outputName + "\n"); return;
            }
            if (!disk.writeFile(outputName, executable)) { appendOutput("ASM16 ERROR: Disk full\n"); return; }
            appendOutput("ASM16: created " + outputName + "\n");
            beginPrompt(Prompt::CompilerCommand, "ASM16> compile <source.asm> [-o <program.exe>]");
        }
    }

    void runExecutable(const string& executableName) {
        string source;
        if (!Asm16Compiler::readExecutable(disk.readFile(executableName), source)) {
            appendOutput("ERROR: " + executableName + " is not an ASM16 executable\n"); return;
        }
        statusBar = "EXECUTING";
        appendOutput("=== " + executableName + " ===\n");
        constexpr size_t maxInstructions = 100000;
        for (size_t step = 0; step < maxInstructions; ++step) {
            appendOutput(cpu.executeProgram(source));
            if (cpu.isWaitingForTerminalInput()) {
                statusBar = "WAITING FOR INPUT";
                beginPrompt(Prompt::PortInput, "PROGRAM INPUT FOR PORT 0x" + to_string(cpu.getWaitingInputPort()) + ":");
                return;
            }
            if (!cpu.hasProgram()) { appendOutput("[Program complete]\n"); statusBar = "ONLINE"; return; }
        }
        cpu.stopProgram();
        appendOutput("ERROR: execution stopped after 100000 instructions\n");
        statusBar = "ONLINE";
    }

    void createProgram() { beginPrompt(Prompt::FileName, "NEW FILE NAME (.asm):"); }

    void openSelectedFile() {
        auto entries = disk.getCurrentEntries();
        if (entries.empty() || fileSelection >= (int)entries.size()) return;
        const auto& entry = entries[fileSelection];
        if (entry.isDirectory) { disk.changeDirectory(entry.path); fileSelection = 0; return; }
        // Each file has its own clean terminal session; do not carry output between files.
        outputBuffer.clear();
        if (fs::path(entry.path).filename() == "asm16.exe") {
            screen = Screen::Editor; showEditor = false;
            appendOutput("ASM16 compiler console opened.\n");
            beginPrompt(Prompt::CompilerCommand, "ASM16> compile <source.asm> [-o <program.exe>]");
            return;
        }
        if (fs::path(entry.path).extension() == ".exe") {
            cpu.stopProgram();
            activeExecutable = entry.path;
            screen = Screen::Editor; showEditor = false;
            runExecutable(activeExecutable);
            return;
        }
        editorFileName = entry.path;
        editor = disk.readFile(entry.path);
        cursorLine = cursorCol = 0;
        updateEditorLines();
        screen = Screen::Editor;
        showEditor = true;
        appendOutput("Opened: " + editorFileName + "\n");
    }

    void saveProgram() {
        if (editorFileName.empty()) { createProgram(); return; }
        if (disk.writeFile(editorFileName, editor))
            appendOutput("Saved to HDD: " + editorFileName + "\n");
        else
            appendOutput("ERROR: Could not save " + editorFileName + " (disk full?)\n");
    }

    // Delete the currently selected entry in the browser.
    void deleteSelected() {
        auto entries = disk.getCurrentEntries();
        if (entries.empty() || fileSelection >= (int)entries.size()) return;
        const auto& entry = entries[fileSelection];
        string name = disk.getName(entry.path);
        bool ok = entry.isDirectory ? disk.deleteDirectory(entry.path)
                                    : disk.deleteFile(entry.path);
        if (ok) {
            appendOutput(string("Deleted: ") + (entry.isDirectory ? "[DIR] " : "[ASM] ") + name + "\n");
            fileSelection = max(0, fileSelection - 1);
        } else {
            appendOutput("ERROR: Could not delete " + name + "\n");
        }
    }

    static void errorCallback(int, const char* description) {
        cerr << "GLFW error: " << description << '\n';
    }

    static void keyCallback(GLFWwindow* window, int key, int, int action, int mods) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        auto* app = static_cast<TerminalInterface*>(glfwGetWindowUserPointer(window));
        if (!app) return;

        if (key == GLFW_KEY_F11) { app->togglePower(); return; }
        if (!app->isPowered || app->booting) return;
        app->cpu.setLastKeyScanCode(static_cast<uint16_t>(key));
        if (app->prompt != Prompt::None) {
            if (key == GLFW_KEY_ESCAPE) { app->prompt = Prompt::None; return; }
            if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { app->finishPrompt(); return; }
            if (key == GLFW_KEY_BACKSPACE && !app->promptValue.empty()) app->promptValue.pop_back();
            return;
        }

        if (key == GLFW_KEY_F1) { app->showHelp = !app->showHelp; return; }
        if (app->showHelp) {
            if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER) app->showHelp = false;
            return;
        }

        if (app->screen == Screen::Menu) {
            if (key == GLFW_KEY_UP) app->menuSelection = (app->menuSelection + 2) % 3;
            else if (key == GLFW_KEY_DOWN) app->menuSelection = (app->menuSelection + 1) % 3;
            else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                if (app->menuSelection == 0) app->screen = Screen::Browser;
                else if (app->menuSelection == 1) app->createProgram();
                else app->appendOutput(app->disk.listFiles());
            }
            return;
        }
        if (app->screen == Screen::Browser) {
            auto entries = app->disk.getCurrentEntries();
            if (key == GLFW_KEY_ESCAPE) app->screen = Screen::Menu;
            else if (key == GLFW_KEY_UP && !entries.empty()) app->fileSelection = max(0, app->fileSelection - 1);
            else if (key == GLFW_KEY_DOWN && !entries.empty()) app->fileSelection = min((int)entries.size() - 1, app->fileSelection + 1);
            else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) app->openSelectedFile();
            else if (key == GLFW_KEY_F4) app->createProgram();
            else if (key == GLFW_KEY_F10 || key == GLFW_KEY_DELETE) app->deleteSelected();
            else if (key == GLFW_KEY_BACKSPACE && app->disk.getCurrentDirectory() != "/") {
                app->disk.changeDirectory("/"); app->fileSelection = 0;
            }
            return;
        }
        // Editor / general shortcuts
        if (key == GLFW_KEY_F5) app->saveProgram();
        else if (key == GLFW_KEY_F2) app->resetMachine();
        else if (key == GLFW_KEY_F3) app->appendOutput(app->disk.listFiles());
        else if (key == GLFW_KEY_F4) app->createProgram();
        else if (key == GLFW_KEY_F6) app->saveProgram();
        else if (key == GLFW_KEY_F7) { app->screen = Screen::Browser; app->showEditor = false; }
        else if (key == GLFW_KEY_ESCAPE) { app->screen = Screen::Browser; app->showEditor = false; }
        else if (key == GLFW_KEY_F8) app->beginPrompt(Prompt::PortInput, "PORT INPUT (0x00 STRING / 0x02 ASCII):");
        else if (key == GLFW_KEY_F9) app->beginPrompt(Prompt::DirectoryName, "NEW DIRECTORY NAME:");
        else if (key == GLFW_KEY_BACKSPACE) app->deleteChar();
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) app->insertChar('\n');
        else if (key == GLFW_KEY_UP) app->moveCursorUp();
        else if (key == GLFW_KEY_DOWN) app->moveCursorDown();
        else if (key == GLFW_KEY_LEFT) app->moveCursorLeft();
        else if (key == GLFW_KEY_RIGHT) app->moveCursorRight();
        else if (key == GLFW_KEY_TAB) {
            app->insertChar(' '); app->insertChar(' ');
            app->insertChar(' '); app->insertChar(' ');
        }
    }

    static void charCallback(GLFWwindow* window, unsigned int codepoint) {
        auto* app = static_cast<TerminalInterface*>(glfwGetWindowUserPointer(window));
        if (!app || app->showHelp || !app->isPowered || app->booting || codepoint < 32 || codepoint > 126) return;
        if (app->prompt != Prompt::None) { app->promptValue += static_cast<char>(codepoint); return; }
        app->insertChar(static_cast<char>(codepoint));
    }

    static const array<uint8_t, 7>& glyph(char c) {
        static const array<uint8_t, 7> empty{};
        static const unordered_map<char, array<uint8_t, 7>> font = {
            {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
            {'C',{0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}}, {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
            {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
            {'G',{0x0F,0x10,0x10,0x17,0x11,0x11,0x0F}}, {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
            {'I',{0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}}, {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
            {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
            {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
            {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
            {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
            {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
            {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
            {'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0A}}, {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
            {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}}, {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
            {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
            {'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}}, {'3',{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}},
            {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, {'5',{0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}},
            {'6',{0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}}, {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
            {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, {'9',{0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}},
            {'-',{0,0,0,0x1F,0,0,0}}, {'_',{0,0,0,0,0,0,0x1F}}, {'=',{0,0x1F,0,0x1F,0,0,0}},
            {'.',{0,0,0,0,0,0x06,0x06}}, {':',{0,0x06,0x06,0,0x06,0x06,0}}, {'/',{0x01,0x02,0x04,0x08,0x10,0,0}},
            {'[',{0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}}, {']',{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}},
            {'!',{0x04,0x04,0x04,0x04,0x04,0,0x04}}, {'>',{0x10,0x08,0x04,0x02,0x04,0x08,0x10}},
            {'<',{0x01,0x02,0x04,0x08,0x04,0x02,0x01}}, {'?',{0x0E,0x11,0x01,0x02,0x04,0,0x04}},
            {'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}}, {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
            {',',{0,0,0,0,0x06,0x06,0x04}}, {';',{0,0x06,0x06,0,0x06,0x06,0x04}},
            {'*',{0,0x15,0x0E,0x1F,0x0E,0x15,0}}, {'+',{0,0x04,0x04,0x1F,0x04,0x04,0}},
            {'|',{0x04,0x04,0x04,0x04,0x04,0x04,0x04}}
        };
        auto it = font.find(static_cast<char>(toupper(static_cast<unsigned char>(c))));
        return it == font.end() ? empty : it->second;
    }

    static void rect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
        glColor4f(r, g, b, a); glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
    }

    static void text(float x, float y, const string& value, float scale, float r, float g, float b) {
        const float start = x, advance = 6.0f * scale, line = 9.0f * scale;
        glColor3f(r, g, b); glBegin(GL_QUADS);
        for (char c : value) {
            if (c == '\n') { x = start; y += line; continue; }
            for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
                if (glyph(c)[row] & (1 << (4 - col))) {
                    const float px = x + col * scale, py = y + row * scale;
                    glVertex2f(px, py); glVertex2f(px + scale, py);
                    glVertex2f(px + scale, py + scale); glVertex2f(px, py + scale);
                }
            x += advance;
        }
        glEnd();
    }

    static vector<string> linesFor(const string& value, size_t maxColumns, size_t maxLines) {
        vector<string> lines;
        istringstream stream(value);
        string line;
        while (getline(stream, line) && lines.size() < maxLines) {
            if (line.empty()) { lines.push_back(" "); continue; }
            while (!line.empty() && line.back() == ' ') line.pop_back();
            if (line.empty()) { lines.push_back(" "); continue; }

            while (line.size() > maxColumns && lines.size() < maxLines) {
                lines.push_back(line.substr(0, maxColumns));
                line.erase(0, maxColumns);
            }
            if (lines.size() < maxLines) lines.push_back(line);
        }
        return lines;
    }

    void drawEditor(float x, float y, float maxWidth, float maxHeight, float scale) {
        if (!showEditor || editorLines.empty()) return;

        float lineHeight = 9.0f * scale;
        float charWidth = 6.0f * scale;
        size_t maxCols = maxWidth / charWidth;
        size_t maxLines = maxHeight / lineHeight;

        float numWidth = 60 * scale;
        int startLine = editorScroll;
        int endLine = min(startLine + (int)maxLines, (int)editorLines.size());

        rect(x, y, maxWidth, maxHeight, 0.0f, 0.05f, 0.02f, 0.3f);

        float lineY = y;
        for (int i = startLine; i < endLine; i++) {
            string lineNum = to_string(i + 1);
            text(x + 2 * scale, lineY, lineNum, scale * 0.7f, 0.3f, 0.5f, 0.3f);

            string displayLine = editorLines[i];
            if ((int)displayLine.length() > maxCols)
                displayLine = displayLine.substr(0, maxCols);
            text(x + numWidth + 2 * scale, lineY, displayLine, scale, 0.4f, 0.95f, 0.55f);
            lineY += lineHeight;
        }

        if (cursorVisible && cursorLine >= startLine && cursorLine < endLine) {
            float cursorX = x + numWidth + 2 * scale + cursorCol * charWidth;
            float cursorY = y + (cursorLine - startLine) * lineHeight;
            rect(cursorX, cursorY, charWidth * 0.6f, lineHeight, 0.5f, 1.0f, 0.5f, 0.7f);
        }
    }

    void draw(int width, int height) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastUpdateTime);
        lastUpdateTime = currentTime;

        audio.update();

        if (booting && !powerOnSoundPlayed) {
            audio.playPowerOn();
            powerOnSoundPlayed = true;
        }
        if (animating && isPowered && !powerOffSoundPlayed) {
            audio.playPowerOff();
            powerOffSoundPlayed = true;
        }

        if (animating) {
            if (isPowered) {
                powerAnimation -= animationSpeed;
                if (powerAnimation <= 0.0f) { powerAnimation = 0.0f; completePowerToggle(); }
            } else {
                powerAnimation += animationSpeed;
                if (powerAnimation >= 1.0f) {
                    powerAnimation = 1.0f;
                    isPowered = true;
                    cpu.setPowered(true);
                    animating = false;
                }
            }
        }

        if (booting && isPowered) {
            constexpr float bootDurationSeconds = 6.8f;
            bootProgress = min(1.0f, bootProgress + deltaTime / bootDurationSeconds);
            bootMessageTimer += deltaTime;
            if (bootMessageTimer > 0.8f && bootMessageIndex < (int)bootMessages.size() - 1) {
                bootMessageIndex++;
                bootMessageTimer = 0.0f;
            }
            if (bootProgress >= 1.0f) completeBoot();
        }

        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, width, height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();

        glClearColor(0.055f, 0.035f, 0.022f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        for (int y = 0; y < height; y += 6)
            rect(0, static_cast<float>(y), static_cast<float>(width), 1, 0.12f, 0.075f, 0.035f, 0.22f);

        const float scale = min(width / 1280.0f, height / 900.0f);
        const float caseW = 930 * scale, caseH = 745 * scale;
        const float caseX = (width - caseW) / 2, caseY = (height - caseH) / 2 - 16 * scale;
        const float bezel = 58 * scale;
        const float screenX = caseX + bezel, screenY = caseY + 82 * scale;
        const float screenW = caseW - 2 * bezel, screenH = 500 * scale;
        const bool shuttingDown = animating && isPowered && !booting;

        rect(caseX + 16 * scale, caseY + 20 * scale, caseW, caseH, 0.025f, 0.014f, 0.008f, 0.72f);
        rect(caseX, caseY, caseW, caseH, 0.62f, 0.47f, 0.28f);
        rect(caseX + 10 * scale, caseY + 10 * scale, caseW - 20 * scale, caseH - 20 * scale, 0.78f, 0.63f, 0.39f);
        rect(screenX - 16 * scale, screenY - 16 * scale, screenW + 32 * scale, screenH + 32 * scale, 0.13f, 0.10f, 0.055f);
        rect(screenX - 7 * scale, screenY - 7 * scale, screenW + 14 * scale, screenH + 14 * scale, 0.025f, 0.035f, 0.026f);

        float brightness = isPowered ? (1.0f - powerAnimation * 0.5f) : (0.1f + powerAnimation * 0.3f);
        if (booting) brightness = bootProgress * 0.9f + 0.1f;
        if (shuttingDown) brightness = max(0.05f, powerAnimation);

        rect(screenX, screenY, screenW, screenH, 0.0f, 0.075f * brightness, 0.039f * brightness);

        if (booting || !isPowered || (powerAnimation < 0.3f && !shuttingDown)) {
            float alpha = booting ? 1.0f : (isPowered ? 1.0f - powerAnimation * 3.0f : 1.0f);
            rect(screenX, screenY, screenW, screenH, 0.0f, 0.0f, 0.0f, alpha * 0.8f);

            string msg = "POWER OFF";
            if (powerAnimation > 0.3f && !isPowered) msg = "POWERING ON...";
            if (booting) msg = "BOOTING...";

            float msgX = screenX + screenW/2 - msg.length() * 3 * scale;
            float msgY = screenY + screenH/2 - 10 * scale;
            text(msgX, msgY, msg, 2.0f * scale, 0.3f * brightness, 0.3f * brightness, 0.3f * brightness);

            if (!isPowered && powerAnimation < 0.5f) {
                text(screenX + screenW/2 - 100 * scale, screenY + screenH/2 + 20 * scale,
                     "[F11 to power on]", 1.0f * scale, 0.2f, 0.2f, 0.2f);
            }

            if (booting) {
                const float panelX = screenX + screenW * 0.12f;
                const float panelY = screenY + screenH * 0.27f;
                const float panelW = screenW * 0.76f;
                const float panelH = screenH * 0.46f;
                const float pulse = 0.65f + 0.35f * sinf(static_cast<float>(currentTime) * 5.0f);
                const float scanY = panelY + fmodf(static_cast<float>(currentTime) * 80.0f, panelH);

                rect(panelX, panelY, panelW, panelH, 0.0f, 0.08f, 0.035f, 0.8f);
                rect(panelX, panelY, panelW, 2 * scale, 0.18f, 0.9f, 0.48f, 0.9f);
                rect(panelX, panelY + panelH - 2 * scale, panelW, 2 * scale, 0.08f, 0.45f, 0.25f, 0.7f);
                rect(panelX, scanY, panelW, 2 * scale, 0.15f, 0.9f, 0.4f, 0.14f * pulse);

                text(panelX + 22 * scale, panelY + 24 * scale,
                     "CPU-16  /  SYSTEM BOOT", 1.6f * scale, 0.4f, 1.0f, 0.6f);
                text(panelX + 22 * scale, panelY + 48 * scale,
                     "SELF-TEST AND INITIALIZATION", 0.9f * scale, 0.25f, 0.65f, 0.36f);

                float barX = panelX + 22 * scale;
                float barY = panelY + panelH - 46 * scale;
                float barW = panelW - 44 * scale;
                float barH = 10 * scale;

                rect(barX, barY, barW, barH, 0.03f, 0.18f, 0.08f);
                rect(barX, barY, barW * bootProgress, barH, 0.16f, 0.9f, 0.42f);
                rect(barX, barY - 2 * scale, barW * bootProgress, 2 * scale, 0.45f, 1.0f, 0.66f, pulse);

                int msgIndex = min(bootMessageIndex, (int)bootMessages.size() - 1);
                text(panelX + 22 * scale, barY - 28 * scale,
                     "> " + bootMessages[msgIndex], 1.3f * scale, 0.4f, 0.95f, 0.48f);
                text(panelX + panelW - 105 * scale, barY + 22 * scale,
                     to_string(static_cast<int>(bootProgress * 100.0f)) + "% COMPLETE",
                     0.85f * scale, 0.3f, 0.75f, 0.4f);
            }
        } else {
            for (int y = static_cast<int>(screenY); y < screenY + screenH; y += max(2, static_cast<int>(4 * scale)))
                rect(screenX, static_cast<float>(y), screenW, 1, 0.0f, 0.0f, 0.0f, 0.25f);
            rect(screenX, screenY, screenW, 2 * scale, 0.15f, 0.95f, 0.52f, 0.45f);

            const float terminalScale = max(1.1f, 1.6f * scale);

            text(screenX + 18 * scale, screenY + 16 * scale,
                 "CPU-16  //  " + statusBar + "  //  " + disk.getCurrentDirectory(),
                 terminalScale * 0.9f, 0.48f, 1.0f, 0.62f);

            text(screenX + 18 * scale, screenY + 38 * scale,
                 screen == Screen::Editor
                     ? "F5 SAVE  F6 SAVE  F7 FILES  F8 INPUT  F9 MKDIR"
                     : "ENTER SELECT  ARROWS MOVE  F4 NEW  F10 DEL  ESC BACK  F1 HELP  F11 POWER",
                 terminalScale * 0.7f, 0.26f, 0.72f, 0.39f);

            float contentY = screenY + 58 * scale;
            float contentH = screenH - 100 * scale;

            if (screen == Screen::Menu) {
                const array<string, 3> menu = { "BROWSE PROGRAMS", "CREATE PROGRAM", "DISK INFORMATION" };
                text(screenX + 38 * scale, contentY + 22 * scale, "CPU-16 SYSTEM MENU",
                     terminalScale * 1.35f, 0.48f, 1.0f, 0.62f);
                text(screenX + 38 * scale, contentY + 48 * scale, "DRIVE C: 10 MB HARD DISK",
                     terminalScale * 0.75f, 0.28f, 0.72f, 0.39f);
                for (int i = 0; i < 3; ++i) {
                    float y = contentY + (82 + i * 38) * scale;
                    if (i == menuSelection)
                        rect(screenX + 26 * scale, y - 4 * scale, screenW - 52 * scale, 25 * scale,
                             0.08f, 0.34f, 0.16f, 0.7f);
                    text(screenX + 42 * scale, y, string(i == menuSelection ? "> " : "  ") + menu[i],
                         terminalScale, 0.42f, 0.96f, 0.54f);
                }
                text(screenX + 38 * scale, contentY + contentH - 25 * scale,
                     "SELECT AN ITEM TO MANAGE PROGRAMS ON DISC_C.BIN",
                     terminalScale * 0.65f, 0.28f, 0.62f, 0.34f);
            } else if (screen == Screen::Browser) {
                text(screenX + 30 * scale, contentY + 16 * scale, "C: " + disk.getCurrentDirectory(),
                     terminalScale, 0.48f, 1.0f, 0.62f);
                auto entries = disk.getCurrentEntries();
                if (entries.empty())
                    text(screenX + 44 * scale, contentY + 52 * scale, "< EMPTY DIRECTORY >",
                         terminalScale, 0.32f, 0.62f, 0.37f);
                for (size_t i = 0; i < entries.size() && i < 10; ++i) {
                    float y = contentY + (48 + i * 27) * scale;
                    if ((int)i == fileSelection)
                        rect(screenX + 24 * scale, y - 3 * scale, screenW - 48 * scale, 21 * scale,
                             0.08f, 0.34f, 0.16f, 0.7f);
                    string name = disk.getName(entries[i].path);
                    string label = entries[i].isDirectory
                        ? "[DIR]  " + name
                        : (fs::path(name).extension() == ".exe" ? "[EXE]  " : "[ASM]  ") + name + "  " + to_string(entries[i].size) + " BYTES";
                    text(screenX + 38 * scale, y,
                         string((int)i == fileSelection ? "> " : "  ") + label,
                         terminalScale * 0.85f, 0.42f, 0.96f, 0.54f);
                }
                text(screenX + 30 * scale, contentY + contentH - 25 * scale,
                     "ENTER OPEN   F4 NEW   F10 DEL   ESC MENU",
                     terminalScale * 0.65f, 0.28f, 0.62f, 0.34f);
            } else if (showHelp) {
                string help =
                    "=== CPU-16 HELP ===\n\n"
                    "OPEN asm16.exe: compile <source.asm> [-o <program.exe>]\n"
                    "OPEN a program .exe: run it to completion\n"
                    "F8     Set terminal input ports 0x00 / 0x02\n"
                    "F2     Reset system\n"
                    "F3     List directory\n"
                    "F4     Create new file\n"
                    "F6     Save current program to HDD\n"
                    "F7     Return to file browser\n"
                    "F9     Create new directory\n"
                    "F10    Delete selected (in browser)\n"
                    "F11    Toggle power\n"
                    "F1     Toggle help\n"
                    "ESC    Close help\n\n"
                    "Arrow keys to navigate\n"
                    "Type to edit program";
                auto helpLines = linesFor(help, 60, 30);
                float y = contentY + 10 * scale;
                for (const auto& line : helpLines) {
                    text(screenX + 20 * scale, y, line, terminalScale, 0.5f, 1.0f, 0.6f);
                    y += 9 * terminalScale;
                }
            } else {
                float editorH = contentH * 0.5f;
                float outputH = contentH * 0.5f;

                rect(screenX + 10 * scale, contentY, screenW - 20 * scale, editorH, 0.0f, 0.03f, 0.01f, 0.5f);
                drawEditor(screenX + 10 * scale, contentY + 2 * scale,
                          screenW - 20 * scale, editorH - 4 * scale, terminalScale * 0.8f);

                rect(screenX + 10 * scale, contentY + editorH, screenW - 20 * scale, 2 * scale,
                     0.2f, 0.6f, 0.2f, 0.5f);

                float outputY = contentY + editorH + 4 * scale;
                rect(screenX + 10 * scale, outputY, screenW - 20 * scale, outputH - 4 * scale,
                     0.0f, 0.02f, 0.01f, 0.5f);

                string displayOutput = outputBuffer;
                if (displayOutput.length() > 500) {
                    size_t pos = displayOutput.find('\n', displayOutput.length() - 500);
                    if (pos != string::npos) displayOutput = displayOutput.substr(pos + 1);
                }
                auto outLines = linesFor(displayOutput, 80, 20);
                float lineY = outputY + 2 * scale;
                for (const auto& line : outLines) {
                    text(screenX + 14 * scale, lineY, line, terminalScale * 0.7f, 0.4f, 0.8f, 0.4f);
                    lineY += 8 * terminalScale;
                }
            }
        }

        if (prompt != Prompt::None && !shuttingDown) {
            const float panelW = screenW * 0.84f, panelH = 72 * scale;
            const float panelX = screenX + (screenW - panelW) * 0.5f, panelY = screenY + screenH * 0.42f;
            rect(panelX, panelY, panelW, panelH, 0.02f, 0.16f, 0.07f, 0.98f);
            text(panelX + 12 * scale, panelY + 12 * scale, promptText, terminalScale * 0.75f, 0.55f, 1.0f, 0.62f);
            text(panelX + 12 * scale, panelY + 38 * scale, "> " + promptValue + "_", terminalScale, 0.55f, 1.0f, 0.62f);
            text(panelX + 12 * scale, panelY + 57 * scale, "ENTER CONFIRM   ESC CANCEL", terminalScale * 0.55f, 0.32f, 0.7f, 0.39f);
        }

        if (shuttingDown) {
            const float shutdownProgress = 1.0f - powerAnimation;
            const float collapse = max(0.0f, min(1.0f, (shutdownProgress - 0.22f) / 0.78f));
            const float visibleHeight = max(2.0f * scale, screenH * (1.0f - collapse));
            const float lineY = screenY + screenH * 0.5f;
            const float topEdge = lineY - visibleHeight * 0.5f;
            const float bottomEdge = lineY + visibleHeight * 0.5f;
            const float glow = 0.35f + 0.65f * (1.0f - collapse);

            // Небольшой запас по пикселям, чтобы точно перекрыть края.
            const float overlap = 4.0f;

            // alpha = 1.0 — полностью непрозрачный чёрный.
            rect(screenX,
                 screenY - overlap,
                 screenW,
                 max(0.0f, (topEdge - screenY) + overlap),
                 0.0f, 0.0f, 0.0f, 1.0f);

            rect(screenX,
                 bottomEdge,
                 screenW,
                 max(0.0f, (screenY + screenH - bottomEdge) + overlap),
                 0.0f, 0.0f, 0.0f, 1.0f);

            rect(screenX, topEdge, screenW, visibleHeight,
                 0.0f, 0.0f, 0.0f, shutdownProgress * 0.7f);

            rect(screenX, lineY - scale, screenW, 2.0f * scale,
                 0.35f * glow, 1.0f * glow, 0.55f * glow);

            if (shutdownProgress < 0.55f) {
                text(screenX + screenW * 0.5f - 54 * scale, lineY - 26 * scale,
                     "SYSTEM HALT", 1.1f * scale, 0.3f * glow, 0.9f * glow, 0.45f * glow);
            }
        }

        rect(caseX + 34 * scale, caseY + caseH - 120 * scale, 250 * scale, 42 * scale,
             0.48f, 0.35f, 0.20f);
        text(caseX + 48 * scale, caseY + caseH - 107 * scale, "A R C A D E  1 6",
             1.3f * scale, 0.12f, 0.09f, 0.05f);
        text(caseX + 48 * scale, caseY + caseH - 91 * scale, "PERSONAL COMPUTER",
             0.85f * scale, 0.12f, 0.09f, 0.05f);
        for (int i = 0; i < 5; ++i)
            rect(caseX + 38 * scale + i * 10 * scale, caseY + caseH - 55 * scale,
                 6 * scale, 3 * scale, 0.18f, 0.13f, 0.07f);

        float ledBrightness = isPowered ? 1.0f : 0.2f;
        rect(caseX + caseW - 104 * scale, caseY + caseH - 108 * scale, 52 * scale, 26 * scale,
             0.18f, 0.13f, 0.07f);
        rect(caseX + caseW - 97 * scale, caseY + caseH - 101 * scale, 12 * scale, 12 * scale,
             0.22f * ledBrightness, 0.95f * ledBrightness, 0.31f * ledBrightness);
        rect(caseX + caseW - 100 * scale, caseY + caseH - 104 * scale, 18 * scale, 18 * scale,
             0.15f * ledBrightness, 0.5f * ledBrightness, 0.2f * ledBrightness, 0.3f);
        text(caseX + caseW - 166 * scale, caseY + caseH - 68 * scale, "POWER",
             0.9f * scale, 0.18f, 0.13f, 0.07f);

        const float footerY = caseY + caseH - 44 * scale;
        rect(caseX + 122 * scale, footerY, caseW - 244 * scale, 54 * scale,
             0.43f, 0.32f, 0.19f);
        text(caseX + 150 * scale, footerY + 18 * scale,
             "[F11] POWER  [F5] SAVE  [ENTER] OPEN EXE  [F1] HELP",
             1.35f * scale, 0.76f, 0.64f, 0.40f);
    }

public:
    TerminalInterface() : cpu(&disk) {
        if (!disk.fileExists("asm16.exe")) {
            disk.createFile("asm16.exe");
            disk.writeFile("asm16.exe", "ASM16 compiler service: compile <source.asm> [options]");
        }
        updateEditorLines();
    }

    int run() {
        glfwSetErrorCallback(errorCallback);
        if (!glfwInit()) return 1;
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) { glfwTerminate(); return 1; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        window = glfwCreateWindow(mode->width, mode->height, "CPU/16 Green Phosphor", monitor, nullptr);
        if (!window) { glfwTerminate(); return 1; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCharCallback(window, charCallback);

        isPowered = false;
        cpu.setPowered(false);
        statusBar = "OFFLINE";
        powerAnimation = 0.0f;
        animating = false;
        booting = false;
        showEditor = false;
        powerOnSoundPlayed = false;
        powerOffSoundPlayed = false;

        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            if (now - lastBlink > 0.55) {
                cursorVisible = !cursorVisible;
                lastBlink = now;
            }
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            draw(width, height);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }
};

int main() {
    cout << "Executable path: " << getExecutablePath() << "\n";
    cout << "Sounds path: " << getSoundsPath() << "\n";

    string soundsPath = getSoundsPath();
    fs::path sp(soundsPath);
    if (fs::exists(sp)) {
        cout << "Files in sounds directory:\n";
        for (const auto& entry : fs::directory_iterator(sp))
            cout << "  " << entry.path().filename().string() << "\n";
    } else {
        cout << "Sounds directory does not exist!\n";
        if (fs::create_directory(sp)) cout << "Created sounds directory\n";
    }

    TerminalInterface terminal;
    return terminal.run();
}

//g++ -std=c++17 -O2 main.cpp -o cpu_emu.exe -lglfw3 -lopengl32 -lgdi32 -lsfml-audio -lsfml-system -Wno-stringop-overflow
