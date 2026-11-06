#include "storage/DiskImage.h"

#include <array>
#include <limits>

namespace cpu16::storage {
namespace {
constexpr std::array<char, 8> Magic{{'C','P','U','1','6','D','S','K'}};
constexpr std::uint32_t Version = 1;

void put32(std::array<char, 512>& h, std::size_t p, std::uint32_t v) {
    for (unsigned i=0;i<4;++i) h[p+i]=static_cast<char>((v>>(i*8U))&0xFFU);
}
void put64(std::array<char, 512>& h, std::size_t p, std::uint64_t v) {
    for (unsigned i=0;i<8;++i) h[p+i]=static_cast<char>((v>>(i*8U))&0xFFU);
}
std::uint32_t get32(const std::array<char, 512>& h, std::size_t p) {
    std::uint32_t v=0; for(unsigned i=0;i<4;++i) v|=static_cast<std::uint32_t>(static_cast<unsigned char>(h[p+i]))<<(i*8U); return v;
}
std::uint64_t get64(const std::array<char, 512>& h, std::size_t p) {
    std::uint64_t v=0; for(unsigned i=0;i<8;++i) v|=static_cast<std::uint64_t>(static_cast<unsigned char>(h[p+i]))<<(i*8U); return v;
}
bool fail(std::string* e,const char* m){if(e)*e=m;return false;}
}

DiskImage::~DiskImage(){ close(); }

bool DiskImage::create(const std::filesystem::path& path,std::uint64_t bytes,std::string* error){
    close();
    if(bytes==0 || bytes%SectorSize!=0) return fail(error,"Disk size must be a positive multiple of 512 bytes");
    if(bytes>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())-HeaderSize)
        return fail(error,"Disk image is too large for this host");
    std::fstream out(path,std::ios::binary|std::ios::out|std::ios::trunc);
    if(!out) return fail(error,"Cannot create disk image file");
    std::array<char,512> header{};
    for(std::size_t i=0;i<Magic.size();++i) header[i]=Magic[i];
    put32(header,8,Version); put32(header,12,SectorSize); put64(header,16,bytes);
    out.write(header.data(),static_cast<std::streamsize>(header.size()));
    out.seekp(static_cast<std::streamoff>(HeaderSize+bytes-1));
    out.put('\0'); out.flush();
    if(!out) return fail(error,"Failed to initialize disk image");
    return open(path,error);
}

bool DiskImage::open(const std::filesystem::path& path,std::string* error){
    close();
    file_.open(path,std::ios::binary|std::ios::in|std::ios::out);
    if(!file_) return fail(error,"Cannot open disk image");
    std::array<char,512> header{};
    file_.read(header.data(),static_cast<std::streamsize>(header.size()));
    if(!file_ || !std::equal(Magic.begin(),Magic.end(),header.begin()) ||
       get32(header,8)!=Version || get32(header,12)!=SectorSize){
        close(); return fail(error,"Invalid or unsupported CPU16 disk image header");
    }
    sizeBytes_=get64(header,16);
    if(sizeBytes_==0 || sizeBytes_%SectorSize!=0){
        close(); return fail(error,"Invalid disk image capacity");
    }
    file_.seekg(0,std::ios::end);
    const auto actual=static_cast<std::uint64_t>(file_.tellg());
    if(actual<HeaderSize+sizeBytes_){
        close(); return fail(error,"Disk image is truncated");
    }
    path_=path; sectors_=sizeBytes_/SectorSize;
    if(error) error->clear();
    return true;
}

void DiskImage::close() noexcept {
    if(file_.is_open()){file_.flush();file_.close();}
    path_.clear();sizeBytes_=0;sectors_=0;
}
bool DiskImage::isOpen()const noexcept{return file_.is_open();}
std::uint64_t DiskImage::sizeBytes()const noexcept{return sizeBytes_;}
std::uint64_t DiskImage::sectorCount()const noexcept{return sectors_;}
const std::filesystem::path& DiskImage::path()const noexcept{return path_;}

bool DiskImage::readSector(std::uint64_t lba,std::vector<std::uint8_t>& out,std::string* error){
    if(!isOpen())return fail(error,"Disk image is not open");
    if(lba>=sectors_)return fail(error,"Sector address is outside the virtual disk");
    out.resize(SectorSize);
    file_.clear();file_.seekg(static_cast<std::streamoff>(HeaderSize+lba*SectorSize));
    file_.read(reinterpret_cast<char*>(out.data()),SectorSize);
    if(!file_)return fail(error,"Failed to read virtual disk sector");
    if(error)error->clear();return true;
}
bool DiskImage::writeSector(std::uint64_t lba,const std::vector<std::uint8_t>& data,std::string* error){
    if(!isOpen())return fail(error,"Disk image is not open");
    if(lba>=sectors_)return fail(error,"Sector address is outside the virtual disk");
    if(data.size()!=SectorSize)return fail(error,"A disk sector must contain exactly 512 bytes");
    file_.clear();file_.seekp(static_cast<std::streamoff>(HeaderSize+lba*SectorSize));
    file_.write(reinterpret_cast<const char*>(data.data()),SectorSize);
    if(!file_)return fail(error,"Failed to write virtual disk sector");
    if(error)error->clear();return true;
}
bool DiskImage::flush(std::string* error){
    if(!isOpen())return fail(error,"Disk image is not open");
    file_.flush();if(!file_)return fail(error,"Failed to flush virtual disk image");
    if(error)error->clear();return true;
}
} // namespace cpu16::storage
