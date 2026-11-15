#pragma once
#include <string>
namespace stickychat {
bool loadProviderKey(std::string& key);
bool saveProviderKey(const std::string& key);
bool removeProviderKey();
}
