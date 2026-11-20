#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#if __has_include("PrivateState.h")
#include "PrivateState.h"
int main(){
 using namespace crosscub::findmy;
 ::findmy::Provision p;
 std::string input="{\"schema\":\"crosscub-findmy-v1\",\"master_private_key_hex\":\""+std::string(55,'0')+"1\",\"primary_ratchet_seed_hex\":\""+std::string(64,'5')+"\",\"paired_at\":\"2000-01-01T00:00:00Z\"}";
 assert(parseImport(input.data(),input.size(),p));assert(p.epochSeconds==946684800);
 assert(!parseImport(input.data(),input.size()-1,p));
 input.insert(1,"\"extra\":1,");assert(!parseImport(input.data(),input.size(),p));
 ::findmy::Checkpoint cp;cp.index=42;cp.epochSeconds=1800000000;cp.seed.fill(9);
 StateBytes bytes{}; encodeState(cp,bytes);
 ::findmy::Checkpoint out;assert(decodeState(bytes,out));assert(out.index==42&&out.seed==cp.seed);
 bytes.back()^=1;assert(!decodeState(bytes,out));bytes.back()^=1;
 bytes[25]^=1;assert(!decodeState(bytes,out));bytes[0]=9;assert(!decodeState(bytes,out));
 puts("PASS bounded strict import / versioned checkpoint corruption rejection");
}
#else
int main(){assert(false && "private import/checkpoint codec missing");}
#endif
