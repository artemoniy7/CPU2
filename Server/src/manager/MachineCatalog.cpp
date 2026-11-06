#include "manager/MachineCatalog.h"
#include "storage/DiskImage.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
namespace cpu16::manager {
namespace {
bool fail(std::string* e,const char* m){if(e)*e=m;return false;}
std::filesystem::path configPath(const std::filesystem::path& root,const std::string& id){return root/"machines"/(id+".machine");}
bool validId(const std::string& id){
 if(id.empty()||id.size()>48)return false;
 for(char c:id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
 return true;
}
bool readRecord(const std::filesystem::path& p,MachineRecord& r){
 std::ifstream in(p);std::string magic;unsigned version=0;
 if(!(in>>magic>>version)||magic!="CPU16_MACHINE"||version!=1)return false;
 if(!(in>>std::quoted(r.id)>>std::quoted(r.config.name)>>std::quoted(r.config.cpuModel)>>r.config.ramSizeBytes>>r.config.cyclesPerSlice>>r.diskSizeBytes>>std::quoted(r.diskImage)))return false;
 std::string error;return validId(r.id)&&r.config.isValid(&error)&&r.diskSizeBytes>0&&r.diskSizeBytes%storage::DiskImage::SectorSize==0;
}
}
MachineCatalog::MachineCatalog(std::filesystem::path root):root_(std::move(root)){}
bool MachineCatalog::initialize(std::string* error){
 std::error_code ec;std::filesystem::create_directories(root_/"machines",ec);
 if(ec)return fail(error,"Cannot create server data directory");
 std::filesystem::create_directories(root_/"disks",ec);
 if(ec)return fail(error,"Cannot create virtual disk directory");
 if(error)error->clear();return true;
}
bool MachineCatalog::create(const MachineRecord& r,std::string* error){
 std::string why;if(!validId(r.id))return fail(error,"Machine ID may contain only letters, digits, '-' and '_'");
 if(!r.config.isValid(&why))return fail(error,why.c_str());
 if(r.diskSizeBytes==0||r.diskSizeBytes%storage::DiskImage::SectorSize)return fail(error,"Disk capacity must be a multiple of 512 bytes");
 if(std::filesystem::exists(configPath(root_,r.id)))return fail(error,"A machine with this ID already exists");
 storage::DiskImage disk;
 if(!disk.create(root_/"disks"/r.diskImage,r.diskSizeBytes,&why))return fail(error,why.c_str());
 disk.close();
 std::ofstream out(configPath(root_,r.id),std::ios::trunc);
 if(!out)return fail(error,"Cannot write machine configuration");
 out<<"CPU16_MACHINE 1\n"<<std::quoted(r.id)<<' '<<std::quoted(r.config.name)<<' '<<std::quoted(r.config.cpuModel)<<' '
    <<r.config.ramSizeBytes<<' '<<r.config.cyclesPerSlice<<' '<<r.diskSizeBytes<<' '<<std::quoted(r.diskImage)<<'\n';
 out.flush();if(!out)return fail(error,"Failed to save machine configuration");
 if(error)error->clear();return true;
}
std::vector<MachineRecord> MachineCatalog::list(std::string* error)const{
 std::vector<MachineRecord> result;std::error_code ec;
 const auto dir=root_/"machines";
 if(!std::filesystem::exists(dir,ec)){if(error)error->clear();return result;}
 for(const auto& entry:std::filesystem::directory_iterator(dir,ec)){
  if(ec)break;if(!entry.is_regular_file()||entry.path().extension()!=".machine")continue;
  MachineRecord r;if(readRecord(entry.path(),r))result.push_back(std::move(r));
 }
 if(error){if(ec)*error="Failed to enumerate machine catalog";else error->clear();}
 return result;
}
bool MachineCatalog::find(const std::string& id,MachineRecord& r,std::string* error)const{
 if(!validId(id))return fail(error,"Invalid machine ID");
 if(!readRecord(configPath(root_,id),r))return fail(error,"Machine not found or configuration is invalid");
 if(error)error->clear();return true;
}
}
