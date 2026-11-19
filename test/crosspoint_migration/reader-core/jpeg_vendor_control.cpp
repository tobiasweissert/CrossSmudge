// Read-only vendor control: deliberately no production Crosscub converter source.
#include <JPEGDEC.h>
#include <fstream>
#include <vector>
#include <cassert>
#include <memory>
int main(int argc,char**argv){assert(argc==2);std::ifstream file(argv[1],std::ios::binary);assert(file);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});auto jpeg=std::make_unique<JPEGDEC>();assert(jpeg->openRAM(bytes.data(),bytes.size(),nullptr)==1);jpeg->close();}
