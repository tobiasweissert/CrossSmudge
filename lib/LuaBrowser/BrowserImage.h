#pragma once
#include <string>
class GfxRenderer;
namespace ink::browser {
// Caller resolves the session-confined path. Decoder/factory access must be
// serialized by LuaAppActivity's render mutex. No retained file/decoder handle.
bool drawImage(const std::string& path,GfxRenderer& renderer,int x,int y,int width,int height);
}
