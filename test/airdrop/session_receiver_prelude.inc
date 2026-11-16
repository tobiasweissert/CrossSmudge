// Hardware/crypto/socket boundaries only. Bodies tested below are extracted verbatim.
#include "Receiver.h"
#include "AllocationBudget.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cstdio>
#define LOG_ERR(...) ((void)0)
#define portENTER_CRITICAL(...) ((void)0)
#define portEXIT_CRITICAL(...) ((void)0)
constexpr int PSA_SUCCESS=0,PSA_KEY_TYPE_RAW_DATA=1;
using psa_key_id_t=uint32_t;
struct psa_key_attributes_t{int value;};
#define PSA_KEY_ATTRIBUTES_INIT {0}
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MBEDTLS_SSL_IS_SERVER=1,MBEDTLS_SSL_TRANSPORT_STREAM=2,MBEDTLS_SSL_PRESET_DEFAULT=3,MBEDTLS_SSL_VERIFY_NONE=0,MBEDTLS_SSL_VERSION_TLS1_2=12;
static unsigned imports=0,destroys=0,maxLive=0,resets=0,budgetBinds=0,budgetEnds=0,initCalls=0;
static int importFailure=0,destroyFailure=0;
static bool psaFailure=false,psaReady=false,volatileReady=false;
static uint32_t keySerial=1;
static std::set<uint32_t> liveKeys;
static std::vector<std::string> calls;
static std::string failName;
static ink::http::AllocationBudget* activeBudget=nullptr;
static unsigned chargedGlobals=0;
static void persistentAllocation(){if(activeBudget){++chargedGlobals;assert(activeBudget->add(reinterpret_cast<void*>(uintptr_t(0x1000+chargedGlobals)),64));}}
int psa_crypto_init(){calls.push_back("psa-init");if(psaFailure)return 1;if(!psaReady){psaReady=true;persistentAllocation();}return 0;}
void psa_set_key_type(psa_key_attributes_t*,int){}
void psa_set_key_bits(psa_key_attributes_t*,int){}
int psa_import_key(psa_key_attributes_t*,const uint8_t*,size_t,psa_key_id_t* out){
  ++imports;*out=0;if(int(imports)==importFailure)return 1;
  if(!volatileReady){volatileReady=true;persistentAllocation();}
  *out=keySerial++;liveKeys.insert(*out);maxLive=std::max(maxLive,unsigned(liveKeys.size()));return 0;
}
int psa_destroy_key(psa_key_id_t key){++destroys;if(int(destroys)==destroyFailure)return 1;assert(liveKeys.erase(key)==1);return 0;}
void psa_reset_key_attributes(psa_key_attributes_t*){++resets;}
namespace HalAirDropRadio {
bool beginTlsBudget(ink::http::AllocationBudget* budget){calls.push_back("budget-begin");assert(!activeBudget);activeBudget=budget;++budgetBinds;return true;}
void endTlsBudget(){calls.push_back("budget-end");assert(activeBudget);activeBudget=nullptr;++budgetEnds;}
}
size_t heap_caps_get_free_size(int){return 200*1024;}
size_t heap_caps_get_largest_free_block(int){return 80*1024;}
int mbedtls_entropy_func(void*,unsigned char*,size_t){return 0;}
int mbedtls_ctr_drbg_random(void*,unsigned char*,size_t){return 0;}
static int call(const char* name){calls.emplace_back(name);return failName==name?1:0;}
#define CRYPTO_STUB(name) template<class... A>int name(A...){return call(#name);}
CRYPTO_STUB(mbedtls_ssl_init)
CRYPTO_STUB(mbedtls_ssl_config_init)
CRYPTO_STUB(mbedtls_x509_crt_init)
CRYPTO_STUB(mbedtls_pk_init)
CRYPTO_STUB(mbedtls_entropy_init)
CRYPTO_STUB(mbedtls_ctr_drbg_init)
CRYPTO_STUB(mbedtls_net_init)
CRYPTO_STUB(mbedtls_ctr_drbg_seed)
CRYPTO_STUB(mbedtls_x509_crt_parse_der)
CRYPTO_STUB(mbedtls_pk_parse_key)
CRYPTO_STUB(mbedtls_ssl_config_defaults)
CRYPTO_STUB(mbedtls_ssl_conf_authmode)
CRYPTO_STUB(mbedtls_ssl_conf_rng)
CRYPTO_STUB(mbedtls_ssl_conf_min_tls_version)
CRYPTO_STUB(mbedtls_ssl_conf_own_cert)
CRYPTO_STUB(mbedtls_ssl_setup)
CRYPTO_STUB(mbedtls_ssl_free)
CRYPTO_STUB(mbedtls_ssl_config_free)
CRYPTO_STUB(mbedtls_pk_free)
CRYPTO_STUB(mbedtls_x509_crt_free)
CRYPTO_STUB(mbedtls_ctr_drbg_free)
CRYPTO_STUB(mbedtls_entropy_free)
static const uint8_t AIRDROP_CRT_EC_DER[1]={},AIRDROP_KEY_EC_DER[1]={};
constexpr size_t AIRDROP_CRT_EC_DER_LEN=1,AIRDROP_KEY_EC_DER_LEN=1;
static std::vector<int> closed;
int close(int fd){closed.push_back(fd);return 0;}
size_t strlcpy(char* out,const char* src,size_t n){size_t size=std::strlen(src);if(n){size_t amount=std::min(size,n-1);std::memcpy(out,src,amount);out[amount]=0;}return size;}
static unsigned socketCalls=0;
static bool reused=false,boundInterface=false,interfaceFailure=false,socketBindFailure=false;
struct sockaddr {};
struct sockaddr_in6 {int sin6_family=0;uint16_t sin6_port=0;uint32_t sin6_scope_id=0;uint8_t sin6_addr[16]{};};
uint16_t htons(uint16_t n){return uint16_t((n>>8)|(n<<8));}
constexpr int AF_INET6=10,SOCK_STREAM=1,ESP_OK=0;
int socket(int domain,int type,int protocol){assert(domain==AF_INET6&&type==SOCK_STREAM&&protocol==0);calls.push_back("socket-create");return 71;}
int bind(int,const sockaddr* address,unsigned size){assert(reused&&boundInterface&&size==sizeof(sockaddr_in6));auto* local=reinterpret_cast<const sockaddr_in6*>(address);assert(local->sin6_scope_id==3&&local->sin6_port==htons(8770));calls.push_back("socket-bind");return socketBindFailure?-1:0;}
int listen(int,int){calls.push_back("socket-listen");return 0;}
uint32_t awdl_netif_index(){return 3;}
void* awdl_netif(){return reinterpret_cast<void*>(uintptr_t(1));}
int esp_netif_get_netif_impl_name(void*,char* name){if(interfaceFailure)return 1;std::strcpy(name,"aw0");return 0;}

int setsockopt(int,int level,int option,const void* value,unsigned length){
  ++socketCalls;assert(level==SOL_SOCKET);
  if(option==SO_REUSEADDR){assert(length==sizeof(int)&&*static_cast<const int*>(value)==1);reused=true;calls.push_back("socket-reuse");return 0;}
  assert(option==SO_BINDTODEVICE&&length==sizeof(ifreq));
  auto* request=static_cast<const ifreq*>(value);assert(std::strcmp(request->ifr_name,"aw0")==0);boundInterface=true;return 0;
}
namespace airdrop {
namespace {
std::atomic<bool> running{false};
Status status;
static uint64_t clock=1000;
uint64_t clockMs(){return clock;}
struct Context {
  ink::http::AllocationBudget budget;
  int ssl=0,config=0,certificate=0,key=0,entropy=0,random=0;
  struct {int fd=-1;} net;
  int client=-1,listener=-1;
  struct {uint8_t ll[16]{};} address;
  char interfaceName[8]{};
  bool tlsInitialized=false,budgetBound=false,failed=false;
};
bool nonblocking(int){calls.push_back("nonblocking");return true;}
