#pragma once
#include "LuaHttp.h"
namespace ink::http {
// The same production HTTP parser/TLS Io is used for in-memory and file responses.
void exchangeDownload(Job&, Io&, DownloadFiles&);
}
