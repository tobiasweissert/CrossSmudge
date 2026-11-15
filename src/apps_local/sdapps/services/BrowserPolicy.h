#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace stickyweb {
struct Destination { std::string host, target; };
bool parseDestination(std::string_view url, Destination& out);
bool publicIpv4(uint32_t address); // host-order, first octet in high byte
}
