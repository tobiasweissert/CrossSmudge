#pragma once
#include <string>
#include <string_view>
namespace stickyweb {
constexpr size_t kMaxBody=32768;
struct HttpResponse { bool ok=false; int status=0; std::string body, location, error; };
HttpResponse decodeResponse(std::string_view raw, bool cleanClose);
}
