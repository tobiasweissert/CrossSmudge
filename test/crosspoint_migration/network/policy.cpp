#include "CrosscubOtaPolicy.h"
#include "HttpRedirectPolicy.h"
#include "ProtectedPaths.h"
#include "version.inc"
#include "credential.inc"
#include <cassert>
#include <string>
#include <iostream>
int main(){
 const char* tag="crosscub-v1.6.5.6";
 const char* url="https://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.6/crosscub-sticky-v1.6.5.6.bin";
 const std::string digest(64,'a');
 assert(crosscub_ota::acceptsDownload(tag,url,digest.c_str(),65536));
 for(const char* invalid:{"crosscub-v1.6.5.5","1.6.5.6","crosscub-v1/evil","crosscub-v1234567890"})
  assert(!crosscub_ota::acceptsDownload(invalid,url,digest.c_str(),65536));
 for(const char* invalid:{"http://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.6/crosscub-sticky-v1.6.5.6.bin","https://github.com/attacker/Crosscub/releases/download/crosscub-v1.6.5.6/crosscub-sticky-v1.6.5.6.bin","https://github.com/tobiasweissert/Crosscub/releases/download/crosscub-v1.6.5.6/crosscub-sticky-v1.6.5.6.bin?other"})
  assert(!crosscub_ota::acceptsDownload(tag,invalid,digest.c_str(),65536));
 assert(!crosscub_ota::acceptsDownload(tag,url,"bad",65536));
 assert(!crosscub_ota::acceptsDownload(tag,url,digest.c_str(),65535));
 uint8_t descriptor[112]{};descriptor[0]=0xe9;descriptor[32]=0x32;descriptor[33]=0x54;descriptor[34]=0xcd;descriptor[35]=0xab;
 memcpy(descriptor+80,"Crosscub",9);memcpy(descriptor+48,"1.6.5.6",8);
 assert(crosscub_ota::acceptsImageVersion(descriptor,sizeof descriptor,tag,"1.6.5.5",compareVersions));
 assert(!crosscub_ota::acceptsImageVersion(descriptor,sizeof descriptor,tag,"1.6.5.6",compareVersions));
 assert(!crosscub_ota::acceptsImageVersion(descriptor,sizeof descriptor,"crosscub-v1.6.5.7","1.6.5.5",compareVersions));
 descriptor[80]='X';assert(!crosscub_ota::acceptsImageVersion(descriptor,sizeof descriptor,tag,"1.6.5.5",compareVersions));descriptor[80]='C';
 memset(descriptor+48,'1',32);assert(!crosscub_ota::acceptsImageVersion(descriptor,sizeof descriptor,tag,"1.6.5.5",compareVersions));
 for(const char* path:{"/.crosspoint/wifi.json","//.crosspoint/./koreader.json.old","/.crosspoint/../.CROSSPOINT/OPDS.JSON","/CROSSP~1/WIFI.JSON","/.crosspoint/opds_tokens.json", "/.crosspoint/lcp_passphrases.json.old", "/.CROSSPOINT/OPDS_TOKENS.JSON. "}) assert(networkCredentialPath(path));
 assert(!protectedpaths::isSensitivePath("/books/public.epub"));
 HttpRedirectPolicy::Url first,other;
 assert(HttpRedirectPolicy::parseUrl("https://example.org/book",first));
 assert(HttpRedirectPolicy::parseUrl("https://EXAMPLE.org:443/other",other));
 assert(HttpRedirectPolicy::shouldSendAuthorization(other,first,true));
 assert(HttpRedirectPolicy::parseUrl("http://example.org/book",other));
 assert(!HttpRedirectPolicy::isAllowedRedirect(first,other));
 assert(!HttpRedirectPolicy::shouldSendAuthorization(other,first,true));
 std::cout<<"production OTA manifest/descriptor version preflight, credential aliases, redirect policy PASS\n";
}
