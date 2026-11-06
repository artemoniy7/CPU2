#include "manager/MachineCatalog.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc,char** argv){
 using namespace cpu16;
 const auto root=std::filesystem::current_path()/"cpu16-server-data";
 manager::MachineCatalog catalog(root);
 std::string error;
 if(!catalog.initialize(&error)){std::cerr<<"Server Manager: "<<error<<'\n';return EXIT_FAILURE;}
 if(argc<2||std::string(argv[1])=="help"){
  std::cout<<"CPU16 Server Manager 0.1\n"
           <<"  cpu16-server list\n"
           <<"  cpu16-server create <id> <name> [ram-MiB] [disk-MiB]\n"
           <<"  cpu16-server info <id>\n"
           <<"Data directory: "<<root.string()<<"\n";
  return EXIT_SUCCESS;
 }
 const std::string command=argv[1];
 if(command=="list"){
  const auto records=catalog.list(&error);
  if(!error.empty()){std::cerr<<error<<'\n';return EXIT_FAILURE;}
  std::cout<<"CPU16 SERVER MANAGER — VIRTUAL MACHINES\n";
  if(records.empty())std::cout<<"  No virtual machines yet. Use: cpu16-server create <id> <name>\n";
  for(const auto& r:records)std::cout<<"  "<<r.id<<" | "<<r.config.name<<" | CPU "<<r.config.cpuModel
      <<" | RAM "<<(r.config.ramSizeBytes/(1024*1024))<<" MiB | Disk "<<(r.diskSizeBytes/(1024*1024))<<" MiB | STOPPED\n";
  return EXIT_SUCCESS;
 }
 if(command=="create"){
  if(argc<4){std::cerr<<"Usage: cpu16-server create <id> <name> [ram-MiB] [disk-MiB]\n";return EXIT_FAILURE;}
  manager::MachineRecord r;r.id=argv[2];r.config.name=argv[3];
  try{
   if(argc>4)r.config.ramSizeBytes=std::stoull(argv[4])*1024ULL*1024ULL;
   if(argc>5)r.diskSizeBytes=std::stoull(argv[5])*1024ULL*1024ULL;
  }catch(...){std::cerr<<"RAM and disk sizes must be whole MiB numbers\n";return EXIT_FAILURE;}
  if(!catalog.create(r,&error)){std::cerr<<"Create failed: "<<error<<'\n';return EXIT_FAILURE;}
  std::cout<<"Created virtual machine '"<<r.id<<"'.\n"
           <<"  RAM: "<<(r.config.ramSizeBytes/(1024*1024))<<" MiB\n"
           <<"  Disk image: "<<(r.diskSizeBytes/(1024*1024))<<" MiB\n"
           <<"  State: STOPPED (boot firmware/guest OS is not available yet)\n";
  return EXIT_SUCCESS;
 }
 if(command=="info"){
  if(argc<3){std::cerr<<"Usage: cpu16-server info <id>\n";return EXIT_FAILURE;}
  manager::MachineRecord r;if(!catalog.find(argv[2],r,&error)){std::cerr<<error<<'\n';return EXIT_FAILURE;}
  std::cout<<"Machine: "<<r.config.name<<" ["<<r.id<<"]\nCPU: "<<r.config.cpuModel
           <<"\nRAM: "<<r.config.ramSizeBytes<<" bytes\nDisk: "<<r.diskSizeBytes
           <<" bytes\nDisk image: "<<(root/"disks"/r.diskImage).string()
           <<"\nState: STOPPED\n";
  return EXIT_SUCCESS;
 }
 std::cerr<<"Unknown command. Run cpu16-server help.\n";return EXIT_FAILURE;
}
