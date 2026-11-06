#pragma once
#include "vm/MachineConfig.h"
#include <filesystem>
#include <string>
#include <vector>
namespace cpu16::manager {
struct MachineRecord {
    std::string id;
    vm::MachineConfig config;
    std::uint64_t diskSizeBytes{64ULL*1024ULL*1024ULL};
    std::string diskImage{"disk0.cpu16disk"};
};
class MachineCatalog {
public:
    explicit MachineCatalog(std::filesystem::path root);
    bool initialize(std::string* error=nullptr);
    bool create(const MachineRecord& record,std::string* error=nullptr);
    std::vector<MachineRecord> list(std::string* error=nullptr)const;
    bool find(const std::string& id,MachineRecord& record,std::string* error=nullptr)const;
    const std::filesystem::path& root()const noexcept{return root_;}
private:
    std::filesystem::path root_;
};
}
