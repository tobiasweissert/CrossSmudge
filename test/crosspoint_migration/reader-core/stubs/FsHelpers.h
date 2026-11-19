#pragma once
#include <string>
#include <string_view>
namespace FsHelpers {
inline bool hasJpgExtension(std::string_view s){return s.ends_with(".jpg")||s.ends_with(".jpeg");}
inline bool hasPngExtension(std::string_view s){return s.ends_with(".png");}
inline bool hasBmpExtension(std::string_view s){return s.ends_with(".bmp");}
inline bool hasTxtExtension(std::string_view s){return s.ends_with(".txt");}
inline bool hasMarkdownExtension(std::string_view s){return s.ends_with(".md")||s.ends_with(".markdown");}
inline std::string extractFolderPath(std::string_view s){auto i=s.rfind('/');return i==s.npos?"/":std::string(s.substr(0,i));}
inline std::string getFileNameWithoutExtension(std::string_view s){s=s.substr(s.find_last_of('/')+1);auto i=s.rfind('.');return std::string(s.substr(0,i));}
}
