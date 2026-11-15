Vendored Espressif elf_loader 2.1.0, Apache-2.0.
Archive SHA256: 31a00c76ae203d2f29d257bde2c26fb0d79cb0ee46e3b67e61baa361f01a8e4c
Repository commit cc06c1c383ba78f913596a176a4513c07657ede5.
Local fix: esp_elf.c retains the exact .text section size for symbol mapping;
only text_alloc_size includes word padding. Otherwise adjacent .rodata bytes
can be relocated to the executable alias. The delivered Browser's loading row
exercises this boundary; host-tests/sdapps/loader_probe.c checks it.
library.json selects the Xtensa loader and PSRAM
adapter for the Arduino build. CrossPlayElf.h keeps the public struct layout
consistent with those compilation flags. CrossPlaySymbols.c replaces the broad
upstream default symbol catalogue with memory/string primitives. This is an
admission interface, not a native-code sandbox. No upstream VFS/dlopen APIs
are used for SD reads; the host supplies an immutable HalStorage snapshot.
