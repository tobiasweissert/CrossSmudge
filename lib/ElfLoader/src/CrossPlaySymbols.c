#include <stdint.h>
#include <string.h>
// Only memory/string primitives are imports. All device services use the
// versioned C table; modules cannot accidentally bind a firmware C++ symbol.
uintptr_t elf_find_sym_default(const char *name) {
    if (!strcmp(name, "memcpy")) return (uintptr_t)memcpy;
    if (!strcmp(name, "memmove")) return (uintptr_t)memmove;
    if (!strcmp(name, "memset")) return (uintptr_t)memset;
    if (!strcmp(name, "memcmp")) return (uintptr_t)memcmp;
    if (!strcmp(name, "strlen")) return (uintptr_t)strlen;
    if (!strcmp(name, "strcmp")) return (uintptr_t)strcmp;
    if (!strcmp(name, "strncmp")) return (uintptr_t)strncmp;
    return 0;
}
