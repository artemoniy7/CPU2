#pragma once
#include "storage/DiskImage.h"
#include <cstdint>
#include <string>
#include <vector>
namespace cpu16::storage {
class BlockDevice {
public:
    virtual ~BlockDevice()=default;
    virtual std::uint64_t sectorCount() const noexcept=0;
    virtual bool read(std::uint64_t lba,std::vector<std::uint8_t>& sector,std::string* error=nullptr)=0;
    virtual bool write(std::uint64_t lba,const std::vector<std::uint8_t>& sector,std::string* error=nullptr)=0;
};
class DiskBlockDevice final : public BlockDevice {
public:
    explicit DiskBlockDevice(DiskImage& image):image_(image){}
    std::uint64_t sectorCount()const noexcept override{return image_.sectorCount();}
    bool read(std::uint64_t lba,std::vector<std::uint8_t>& s,std::string* e=nullptr)override{return image_.readSector(lba,s,e);}
    bool write(std::uint64_t lba,const std::vector<std::uint8_t>& s,std::string* e=nullptr)override{return image_.writeSector(lba,s,e);}
private: DiskImage& image_;
};
}
