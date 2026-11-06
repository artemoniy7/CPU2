#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace cpu16::storage {

class DiskImage {
public:
    static constexpr std::uint32_t SectorSize = 512;

    DiskImage() = default;
    ~DiskImage();
    DiskImage(const DiskImage&) = delete;
    DiskImage& operator=(const DiskImage&) = delete;

    bool create(const std::filesystem::path& path, std::uint64_t sizeBytes,
                std::string* error = nullptr);
    bool open(const std::filesystem::path& path, std::string* error = nullptr);
    void close() noexcept;
    bool isOpen() const noexcept;
    std::uint64_t sizeBytes() const noexcept;
    std::uint64_t sectorCount() const noexcept;
    const std::filesystem::path& path() const noexcept;

    bool readSector(std::uint64_t lba, std::vector<std::uint8_t>& out,
                    std::string* error = nullptr);
    bool writeSector(std::uint64_t lba, const std::vector<std::uint8_t>& data,
                     std::string* error = nullptr);
    bool flush(std::string* error = nullptr);

private:
    static constexpr std::uint64_t HeaderSize = 512;
    std::filesystem::path path_;
    std::fstream file_;
    std::uint64_t sizeBytes_{0};
    std::uint64_t sectors_{0};
};

} // namespace cpu16::storage
