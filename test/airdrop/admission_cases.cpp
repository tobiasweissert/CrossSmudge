#include <core/Offer.h>
#include <cassert>
int main(){
 assert(airdrop::safeFilename("ordinary.epub"));
 assert(airdrop::safeFilename("book.txt"));
 assert(!airdrop::safeFilename("book .txt"));
 assert(!airdrop::safeFilename("book..epub"));
 assert(!airdrop::safeFilename("CON.txt"));
}
