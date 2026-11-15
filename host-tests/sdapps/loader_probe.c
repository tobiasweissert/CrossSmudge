/* Loads target instructions but deliberately never executes them on the host. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../lib/ElfLoader/src/esp_elf.c"
static unsigned allocations;
void *esp_elf_malloc(uint32_t n, bool executable) { (void)executable; void* p=malloc(n); if(p) ++allocations; return p; }
void esp_elf_free(void* p) { if(p) --allocations; free(p); }
uintptr_t elf_find_sym_default(const char* name) {
  static const char* allowed[]={"memcpy","memmove","memset","memcmp","strlen","strcmp","strncmp"};
  for(unsigned i=0;i<sizeof(allowed)/sizeof(*allowed);++i)if(!strcmp(name,allowed[i]))return 0x40001000+i*4;
  return 0;
}
int main(int argc, char** argv) {
  assert(argc==2);
  FILE* f=fopen(argv[1],"rb"); assert(f);
  assert(!fseek(f,0,SEEK_END)); long size=ftell(f); assert(size>52 && size<1024*1024);
  rewind(f); unsigned char* bytes=malloc(size); assert(bytes);
  assert(fread(bytes,1,size,f)==(size_t)size); fclose(f);
  for(unsigned i=0;i<100;++i) {
    esp_elf_t elf;
    assert(!esp_elf_init(&elf));
    assert(!esp_elf_relocate(&elf,bytes,size));
    assert(elf.entry);
    // Section ownership must exclude allocation padding. The delivered Browser
    // puts its loading label at .rodata[0], immediately after unaligned .text.
    const esp_elf_sec_t *rodata = &elf.sec[ELF_SEC_RODATA];
    if (rodata->size) {
      uintptr_t mapped = esp_elf_map_sym(&elf, rodata->v_addr);
      if (mapped != rodata->addr) {
        fprintf(stderr, "rodata boundary: vaddr=0x%lx expected=%p actual=%p text-size=0x%lx\n",
                (unsigned long)rodata->v_addr, (void*)rodata->addr, (void*)mapped,
                (unsigned long)elf.sec[ELF_SEC_TEXT].size);
      }
      assert(mapped == rodata->addr);
      assert(esp_elf_map_sym(&elf, rodata->v_addr + rodata->size - 1) ==
             rodata->addr + rodata->size - 1);
    }
    assert(elf.sec[ELF_SEC_TEXT].size);
    // Check relocated data literals as well as the mapping API, using section
    // headers from the input image independently of the loader descriptors.
    const elf32_hdr_t* header = (const elf32_hdr_t*)bytes;
    const elf32_shdr_t* sections = (const elf32_shdr_t*)(bytes + header->shoff);
    unsigned data_literals = 0;
    for (unsigned j = 0; j < header->shnum; ++j) {
      const elf32_shdr_t* section = &sections[j];
      if (section->type != SHT_RELA) continue;
      const elf32_rela_t* relocations = (const elf32_rela_t*)(bytes + section->offset);
      for (unsigned k = 0; k < section->size / sizeof(*relocations); ++k) {
        const elf32_rela_t* rel = &relocations[k];
        if (ELF_R_TYPE(rel->info) != 5) continue; // R_XTENSA_RELATIVE
        for (unsigned q = 0; q < header->shnum; ++q) {
          const elf32_shdr_t* owner = &sections[q];
          if (owner->type != SHT_PROGBITS || rel->offset < owner->addr ||
              rel->offset - owner->addr + 4 > owner->size) continue;
          uint32_t original, actual;
          memcpy(&original, bytes + owner->offset + rel->offset - owner->addr, 4);
          if (original < rodata->v_addr || original - rodata->v_addr >= rodata->size) continue;
          uintptr_t where = esp_elf_map_sym(&elf, rel->offset);
          assert(where);
          memcpy(&actual, (const void*)where, 4);
          assert(actual == (uint32_t)(rodata->addr + original - rodata->v_addr));
          ++data_literals;
        }
      }
    }
    assert(data_literals);

    esp_elf_deinit(&elf);
    assert(!allocations);
  }
  free(bytes);
  puts("100 real S3 ELF load/relocate/unload cycles passed; target instructions not executed");
}
