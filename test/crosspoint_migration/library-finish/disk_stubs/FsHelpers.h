#pragma once
#include <HalStorage.h>
#include <string_view>
#include <cctype>
namespace FsHelpers {
inline bool checkFileExtension(std::string_view p,std::string_view e){if(p.size()<e.size())return false;p.remove_prefix(p.size()-e.size());for(size_t i=0;i<e.size();++i)if(std::tolower(static_cast<unsigned char>(p[i]))!=e[i])return false;return true;}
inline bool hasEpubExtension(std::string_view p){return checkFileExtension(p,".epub");}inline bool hasXtcExtension(std::string_view p){return checkFileExtension(p,".xtc")||checkFileExtension(p,".xtch");}
inline bool directoryIterationFailed(const HalFile&f){return f.iterationFailed();}
}
