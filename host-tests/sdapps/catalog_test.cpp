#include "Catalog.h"
#include <HalStorage.h>
#include <cassert>
#include <cstdio>
static void package(const char* id,const char* manifestId=nullptr){
 auto folder=Storage.root/"apps"/id;std::filesystem::create_directories(folder);
 std::ofstream f(folder/"app.cfg");
 f<<"CROSSPLAY-APP 1\nid="<<(manifestId?manifestId:id)<<"\ntitle=App\ntarget=esp32s3\nabi=1\nimage=app.elf\nsha256="<<std::string(64,'0')<<"\n";
}
int main(int argc,char** argv){
 assert(argc==2);Storage.root=argv[1];std::filesystem::create_directories(Storage.root);
 sdapps::Catalog catalog;
 sdapps::discoverPackages("/apps",catalog);assert(catalog.count==0);
 package("zebra");package("alpha");package("bad","different");package("UPPER");
 auto folder=Storage.root/"apps"/"huge";std::filesystem::create_directories(folder);
 {std::ofstream f(folder/"app.cfg");f<<std::string(321,'x');}
 sdapps::discoverPackages("/apps",catalog);assert(catalog.count==2);
 assert(!strcmp(catalog.entries[0].id,"alpha")&&!strcmp(catalog.entries[1].id,"zebra"));
 sdapps::discoverPackages("/apps/../apps",catalog);assert(catalog.count==0);
 for(int i=0;i<30;++i){char id[20];snprintf(id,sizeof(id),"entry%d",i);package(id);}
 sdapps::discoverPackages("/apps",catalog);assert(catalog.count==16);
}
