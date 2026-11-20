// Algorithm adapted from MIT references; see NOTICE.
#include "FindMyCore.h"
#include <cstring>
#if defined(FINDMY_CRYPTO_OPENSSL) && defined(FINDMY_CRYPTO_MBEDTLS)
#error Select exactly one Find My crypto backend
#endif
#if defined(FINDMY_CRYPTO_OPENSSL)
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#elif defined(FINDMY_CRYPTO_MBEDTLS)
#include <mbedtls/version.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>
#ifndef MBEDTLS_PRIVATE
#define MBEDTLS_PRIVATE(x) x
#endif
#endif
namespace findmy {
void secureErase(void* p, size_t n) {
  volatile uint8_t* b=static_cast<volatile uint8_t*>(p);
  while(n--) *b++=0;
}
bool cryptoAvailable() {
#if defined(FINDMY_CRYPTO_OPENSSL)
  EC_GROUP* g=EC_GROUP_new_by_curve_name(NID_secp224r1);
  bool ok=g!=nullptr; EC_GROUP_free(g); return ok;
#elif defined(FINDMY_CRYPTO_MBEDTLS) && defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED) && defined(MBEDTLS_SHA256_C)
  return true;
#else
  return false;
#endif
}
static bool hash(const uint8_t* input, size_t size, uint8_t* out) {
#if defined(FINDMY_CRYPTO_OPENSSL)
  return SHA256(input,size,out)!=nullptr;
#elif defined(FINDMY_CRYPTO_MBEDTLS) && defined(MBEDTLS_SHA256_C)
#if MBEDTLS_VERSION_MAJOR >= 3
  return mbedtls_sha256(input,size,out,0)==0;
#else
  return mbedtls_sha256_ret(input,size,out,0)==0;
#endif
#else
  (void)input; (void)size; (void)out; return false;
#endif
}
static bool kdf(const Seed& seed, const char* label, size_t length, uint8_t* out, size_t count) {
  // X9.63 SHA256(seed || counter_BE32 || sharedInfo). Only fixed internal labels.
  uint8_t input[45]={}, digest[32]={};
  std::memcpy(input,seed.data(),32); std::memcpy(input+36,label,length);
  uint32_t counter=1; bool ok=true;
  while(count && ok) {
    input[32]=static_cast<uint8_t>(counter>>24); input[33]=static_cast<uint8_t>(counter>>16);
    input[34]=static_cast<uint8_t>(counter>>8); input[35]=static_cast<uint8_t>(counter);
    ok=hash(input,36+length,digest);
    if(ok) { size_t take=count>32?32:count; std::memcpy(out,digest,take); out+=take; count-=take; }
    ++counter;
  }
  secureErase(input,sizeof(input)); secureErase(digest,sizeof(digest)); return ok;
}
bool updateSeed(const Seed& input, Seed& output) {
  Seed next{}; bool ok=kdf(input,"update",6,next.data(),32);
  output=next; secureErase(next.data(),next.size()); return ok;
}
bool validMaster(const MasterKey& master) {
#if defined(FINDMY_CRYPTO_OPENSSL)
  EC_GROUP* g=EC_GROUP_new_by_curve_name(NID_secp224r1);
  BIGNUM* d=BN_bin2bn(master.data(),28,nullptr); BIGNUM* n=BN_new();
  bool ok=g && d && n && EC_GROUP_get_order(g,n,nullptr)==1 && !BN_is_zero(d) && BN_cmp(d,n)<0;
  BN_clear_free(d); BN_clear_free(n); EC_GROUP_free(g); return ok;
#elif defined(FINDMY_CRYPTO_MBEDTLS) && defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
  mbedtls_ecp_group g; mbedtls_ecp_group_init(&g); mbedtls_mpi d; mbedtls_mpi_init(&d);
  int rc=mbedtls_ecp_group_load(&g,MBEDTLS_ECP_DP_SECP224R1);
  if(!rc) rc=mbedtls_mpi_read_binary(&d,master.data(),28);
  if(!rc) rc=mbedtls_ecp_check_privkey(&g,&d);
  mbedtls_mpi_free(&d); mbedtls_ecp_group_free(&g); return rc==0;
#else
  (void)master; return false;
#endif
}
bool derivePublic(const MasterKey& master, const Seed& seed, PublicKey& output, Random rng, void* rngContext) {
  output.fill(0);
  if(!validMaster(master)) return false;
  uint8_t at[72]={};
  if(!kdf(seed,"diversify",9,at,sizeof(at))) { secureErase(at,sizeof(at)); return false; }
  bool ok=false;
#if defined(FINDMY_CRYPTO_OPENSSL)
  (void)rng; (void)rngContext;
  EC_GROUP* g=EC_GROUP_new_by_curve_name(NID_secp224r1); BN_CTX* ctx=BN_CTX_secure_new();
  EC_POINT* point=g?EC_POINT_new(g):nullptr;
  BIGNUM *u=BN_bin2bn(at,36,nullptr), *v=BN_bin2bn(at+36,36,nullptr), *d=BN_bin2bn(master.data(),28,nullptr);
  BIGNUM *n=BN_new(), *nm=BN_new(), *p=BN_new(), *x=BN_new();
  if(d) BN_set_flags(d,BN_FLG_CONSTTIME);
  if(p) BN_set_flags(p,BN_FLG_CONSTTIME);
  ok=g && ctx && point && u && v && d && n && nm && p && x &&
     EC_GROUP_get_order(g,n,ctx)==1 && BN_copy(nm,n) && BN_sub_word(nm,1)==1 &&
     BN_mod(u,u,nm,ctx)==1 && BN_add_word(u,1)==1 && BN_mod(v,v,nm,ctx)==1 && BN_add_word(v,1)==1 &&
     BN_mod_mul(p,u,d,n,ctx)==1 && BN_mod_add(p,p,v,n,ctx)==1 && !BN_is_zero(p) &&
     EC_POINT_mul(g,point,p,nullptr,nullptr,ctx)==1 && EC_POINT_is_on_curve(g,point,ctx)==1 &&
     EC_POINT_get_affine_coordinates(g,point,x,nullptr,ctx)==1 && BN_bn2binpad(x,output.data(),28)==28;
  BN_clear_free(u); BN_clear_free(v); BN_clear_free(d); BN_clear_free(n); BN_clear_free(nm);
  BN_clear_free(p); BN_clear_free(x); EC_POINT_clear_free(point); BN_CTX_free(ctx); EC_GROUP_free(g);
#elif defined(FINDMY_CRYPTO_MBEDTLS) && defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
  // Explicit caller RNG is mandatory for blinding; no Arduino/ESP/global dependency.
  if(rng) {
    mbedtls_ecp_group g; mbedtls_ecp_group_init(&g); mbedtls_ecp_point q; mbedtls_ecp_point_init(&q);
    mbedtls_mpi u,v,d,nm,p; mbedtls_mpi_init(&u); mbedtls_mpi_init(&v); mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&nm); mbedtls_mpi_init(&p);
    int rc=mbedtls_ecp_group_load(&g,MBEDTLS_ECP_DP_SECP224R1);
    if(!rc) rc=mbedtls_mpi_sub_int(&nm,&g.N,1);
    if(!rc) rc=mbedtls_mpi_read_binary(&u,at,36);
    if(!rc) rc=mbedtls_mpi_mod_mpi(&u,&u,&nm);
    if(!rc) rc=mbedtls_mpi_add_int(&u,&u,1);
    if(!rc) rc=mbedtls_mpi_read_binary(&v,at+36,36);
    if(!rc) rc=mbedtls_mpi_mod_mpi(&v,&v,&nm);
    if(!rc) rc=mbedtls_mpi_add_int(&v,&v,1);
    if(!rc) rc=mbedtls_mpi_read_binary(&d,master.data(),28);
    if(!rc) rc=mbedtls_mpi_mul_mpi(&p,&u,&d);
    if(!rc) rc=mbedtls_mpi_add_mpi(&p,&p,&v);
    if(!rc) rc=mbedtls_mpi_mod_mpi(&p,&p,&g.N);
    if(!rc) rc=mbedtls_ecp_check_privkey(&g,&p);
    if(!rc) rc=mbedtls_ecp_mul(&g,&q,&p,&g.G,rng,rngContext);
    if(!rc) rc=mbedtls_ecp_check_pubkey(&g,&q);
    if(!rc) rc=mbedtls_mpi_write_binary(&q.MBEDTLS_PRIVATE(X),output.data(),28);
    ok=rc==0;
    mbedtls_mpi_free(&u); mbedtls_mpi_free(&v); mbedtls_mpi_free(&d); mbedtls_mpi_free(&nm); mbedtls_mpi_free(&p);
    mbedtls_ecp_point_free(&q); mbedtls_ecp_group_free(&g);
  }
#else
  (void)rng; (void)rngContext;
#endif
  secureErase(at,sizeof(at)); if(!ok) output.fill(0); return ok;
}
}
