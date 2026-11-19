#pragma once
// Catalog probe double: deliberately unavailable metadata; production registry must
// retain its filename style fallback. Not a FreeType rasterization substitute.
namespace freeink::font {
class FtFont {
public:
 using ReadFn=unsigned long(*)(void*,unsigned long,unsigned char*,unsigned long);
 struct FaceInfo {unsigned short weight=400;bool italic=false;};
 enum class InspectResult {Ok,Failed};
 static InspectResult inspectStream(ReadFn,void*,unsigned long,FaceInfo&){return InspectResult::Failed;}
};
}
