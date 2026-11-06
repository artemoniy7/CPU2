#include "manager/MachineCatalog.h"
#include "storage/DiskImage.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(){
 namespace fs=std::filesystem;
 try{
  const auto root=fs::temp_directory_path()/"cpu16-server-manager-test";
  fs::remove_all(root);
  cpu16::manager::MachineCatalog catalog(root);std::string error;
  if(!catalog.initialize(&error))throw std::runtime_error(error);
  cpu16::manager::MachineRecord record;record.id="test-vm";record.config.name="Test VM";
  record.config.ramSizeBytes=2*1024*1024;record.diskSizeBytes=2*1024*1024;
  if(!catalog.create(record,&error))throw std::runtime_error(error);
  cpu16::manager::MachineRecord loaded;
  if(!catalog.find("test-vm",loaded,&error))throw std::runtime_error(error);
  if(loaded.config.ramSizeBytes!=record.config.ramSizeBytes)throw std::runtime_error("RAM config mismatch");
  cpu16::storage::DiskImage image;
  if(!image.open(root/"disks"/record.diskImage,&error))throw std::runtime_error(error);
  std::vector<std::uint8_t> sector(512,0x5A);
  if(!image.writeSector(7,sector,&error))throw std::runtime_error(error);
  if(!image.flush(&error))throw std::runtime_error(error);
  std::vector<std::uint8_t> read;
  if(!image.readSector(7,read,&error)||read!=sector)throw std::runtime_error("sector round-trip failed");
  if(image.readSector(image.sectorCount(),read,&error))throw std::runtime_error("out-of-range LBA accepted");
  image.close();fs::remove_all(root);
  std::cout<<"Server manager/storage tests passed.\n";return EXIT_SUCCESS;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return EXIT_FAILURE;}
}
