#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace stickyweb {
struct Link { std::string label; std::string url; };
struct LinkSpan { uint32_t start, end, link; };
struct Document { std::string title; std::string text; std::vector<Link> links; std::vector<LinkSpan> spans; };
struct ParseResult { bool ok; std::string error; Document document; };
// HTTPS only. Invalid/unsupported references return an empty string.
std::string resolveUrl(std::string_view baseUrl, std::string_view reference);
// Inert, bounded, tolerant reader extraction; not an HTML5/CSS renderer.
ParseResult parseHtml(std::string_view html, std::string_view baseUrl);
}
