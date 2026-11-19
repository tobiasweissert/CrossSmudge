// Builds genuine CrossPoint V2 bytes using pinned upstream production TUs.
#include "LibraryBuilder.h"
#include "HalStorage.h"
#include "Epub.h"
#include <fstream>
int main(int argc,char** argv){if(argc!=2)return 2;fake::reset();fake::add("/b.txt","b",2);library::BuildStats s;if(!library::buildLibraryIndex("/",s,false))return 3;fake::add("/a.txt","a",3);if(!library::buildLibraryIndex("/",s,false))return 4;const auto& b=fake::files[library::libraryIndexPath()]->bytes;std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());return out?0:5;}
