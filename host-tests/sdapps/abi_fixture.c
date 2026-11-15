/* Test fixture only: exercises ELF ABI descriptor relocation, not an app. */
#include "AppAbi.h"
static int start(const CpHostV1* h, void** state) { *state = 0; return h->version == CP_APP_ABI_V1; }
static void event(void* s, const CpEvent* e) { (void)s; (void)e; }
static void stop(void* s) { (void)s; }
int app_main(int argc, char** argv) {
  if (argc != 1 || !argv || !argv[0]) return -1;
  CpBootstrap* b = (CpBootstrap*)argv[0];
  if (b->size != sizeof(*b) || b->version != CP_APP_ABI_V1) return -1;
  b->module.size = sizeof(b->module);
  b->module.version = CP_APP_ABI_V1;
  b->module.start = start; b->module.event = event; b->module.stop = stop;
  return 0;
}
