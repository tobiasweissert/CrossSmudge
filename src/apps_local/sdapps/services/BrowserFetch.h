#pragma once
#include <functional>
#include <cstdint>
#include <string>
#include "BrowserHttp.h"
namespace stickyweb {
// Direct transport, no cookies, authentication, proxy or automatic redirects.
HttpResponse fetchOnce(const std::string& url, const std::function<bool()>& cancelled, uint32_t budgetMs=15000);
}
