"""
Apply the pinned AnimatedGIF width, bounds, palette and alignment fixes.
"""

import argparse
import os
import re
import sys


OLD_SNIPPET = """#define MAX_COLORS 256
#ifdef __LINUX__
#define MAX_WIDTH 2048
#else
#define MAX_WIDTH 480
#endif // __LINUX__
"""

NEW_SNIPPET = """#define MAX_COLORS 256
#ifndef MAX_WIDTH
#ifdef __LINUX__
#define MAX_WIDTH 2048
#else
#define MAX_WIDTH 480
#endif // __LINUX__
#endif // MAX_WIDTH
"""

MAX_WIDTH_DEFINE_RE = re.compile(r"^\s*#\s*define\s+MAX_WIDTH\b", re.MULTILINE)
MAX_WIDTH_GUARD_RE = re.compile(
    r"^\s*#\s*(?:ifndef\s+MAX_WIDTH\b|if\s+!\s*defined\s*\(?\s*MAX_WIDTH\s*\)?)",
    re.MULTILINE,
)

PALETTE_OLD = """            // Read enough additional data for the color table
            i = (*pPage->pfnRead)(&pPage->GIFFile, &pPage->ucFileBuf[iBytesRead], 3*(1<<iColorTableBits));
            iBytesRead += i;
            if (iColorTableBits != 1 && i < 3*(1<<iColorTableBits)) { // file too small
                pPage->iError = GIF_BAD_FILE;
                return 0;
            }
"""

PALETTE_NEW = """            // The initial read may already contain the complete color table.
            // Keep the existing read-ahead for the following block parser.
            i = (*pPage->pfnRead)(&pPage->GIFFile, &pPage->ucFileBuf[iBytesRead], 3*(1<<iColorTableBits));
            iBytesRead += i;
            if (iBytesRead < iOffset + 3*(1<<iColorTableBits)) {
                pPage->iError = GIF_BAD_FILE;
                return 0;
            }
"""

COPY_OLD = """            while (buf < pEnd) {
#ifdef ALLOWS_UNALIGNED
// This is a significant perf improvement compared to copying 1 byte at a time
// even though it will often copy too many bytes. Since we're not at the end of
// the line, it's okay to copy a few extra pixels.
                BIGUINT tmp = *(BIGUINT *) s;
                s += sizeof(BIGUINT);
                *(BIGUINT *)buf = tmp;
                buf += sizeof(BIGUINT);
#else
                *buf++ = *s++;
#endif
            }
"""

COPY_PREVIOUS = """#ifdef ALLOWS_UNALIGNED
            while (pEnd - buf >= (int)sizeof(BIGUINT)) {
                BIGUINT tmp = *(BIGUINT *) s;
                s += sizeof(BIGUINT);
                *(BIGUINT *)buf = tmp;
                buf += sizeof(BIGUINT);
            }
#endif
            while (buf < pEnd) {
                *buf++ = *s++;
            }
"""


COPY_NEW = COPY_PREVIOUS.replace(
    "BIGUINT tmp = *(BIGUINT *) s;", "BIGUINT tmp; memcpy(&tmp, s, sizeof(tmp));"
).replace("*(BIGUINT *)buf = tmp;", "memcpy(buf, &tmp, sizeof(tmp));")

ALIGNMENT_OLD = """#if (INTPTR_MAX == INT64_MAX)
#define ALLOWS_UNALIGNED
#define INTELSHORT(p) (*(uint16_t *)p)
#define INTELLONG(p) (*(uint64_t *)p)
#define REGISTER_WIDTH 64
#define BIGINT int64_t
#define BIGUINT uint64_t
#else
#define REGISTER_WIDTH 32
#ifdef ALLOWS_UNALIGNED
#define INTELSHORT(p) (*(uint16_t *)p)
#define INTELLONG(p) (*(uint32_t *)p)
#else
// Due to unaligned memory causing an exception, we have to do these macros the slow way
#define INTELSHORT(p) ((*p) + (*(p+1)<<8))
#define INTELLONG(p) ((*p) + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24))
#endif // ALLOWS_UNALIGNED
#define BIGINT int32_t
#define BIGUINT uint32_t
#endif // 64 vs 32-bit native register size
"""
ALIGNMENT_HELPERS = """// Native-endian loads preserve the existing native fast-path semantics.
// memcpy permits arbitrary byte alignment and avoids typed pointer aliasing.
static inline uint16_t animatedgif_load16(const void *p) {
    uint16_t value; memcpy(&value, p, sizeof(value)); return value;
}
static inline uint32_t animatedgif_load32(const void *p) {
    uint32_t value; memcpy(&value, p, sizeof(value)); return value;
}
static inline uint64_t animatedgif_load64(const void *p) {
    uint64_t value; memcpy(&value, p, sizeof(value)); return value;
}
"""
ALIGNMENT_NEW = ALIGNMENT_HELPERS + ALIGNMENT_OLD.replace(
    "(*(uint16_t *)p)", "animatedgif_load16(p)"
).replace("(*(uint32_t *)p)", "animatedgif_load32(p)").replace(
    "(*(uint64_t *)p)", "animatedgif_load64(p)"
)
TURBO_COPY_OLD = """        BIGUINT tmp = *(BIGUINT *) s;
        s += sizeof(BIGUINT);
        *(BIGUINT *)d = tmp;
        d += sizeof(BIGUINT);
"""
TURBO_COPY_NEW = TURBO_COPY_OLD.replace(
    "BIGUINT tmp = *(BIGUINT *) s;", "BIGUINT tmp; memcpy(&tmp, s, sizeof(tmp));"
).replace("*(BIGUINT *)d = tmp;", "memcpy(d, &tmp, sizeof(tmp));")


def _patch_alignment_header(header_path):
    with open(header_path, "r", encoding="utf-8") as file:
        text = file.read()
    if ALIGNMENT_NEW in text:
        return True
    if text.count(ALIGNMENT_OLD) != 1:
        raise RuntimeError("AnimatedGIF native-load patch does not match %s" % header_path)
    with open(header_path, "w", encoding="utf-8") as file:
        file.write(text.replace(ALIGNMENT_OLD, ALIGNMENT_NEW, 1))
    print("Patched AnimatedGIF alignment-safe native loads")
    return True


def _patch_header(header_path):
    with open(header_path, "r", encoding="utf-8") as file:
        text = file.read()

    max_width_define = MAX_WIDTH_DEFINE_RE.search(text)
    max_width_guard = MAX_WIDTH_GUARD_RE.search(text)
    if NEW_SNIPPET in text or (
        max_width_define
        and max_width_guard
        and max_width_guard.start() < max_width_define.start()
    ):
        return True

    if OLD_SNIPPET not in text:
        if not max_width_define:
            sys.stderr.write(
                "WARNING: AnimatedGIF no longer defines MAX_WIDTH in %s\n"
                % header_path
            )
            return True
        sys.stderr.write(
            "ERROR: AnimatedGIF has an unguarded MAX_WIDTH definition in %s\n"
            % header_path
        )
        raise SystemExit(1)

    with open(header_path, "w", encoding="utf-8") as file:
        file.write(text.replace(OLD_SNIPPET, NEW_SNIPPET, 1))

    print("Patched AnimatedGIF MAX_WIDTH guard")
    return True


def _patch_decoder(source_path):
    with open(source_path, "r", encoding="utf-8") as file:
        original = file.read()
    text = original
    for name, before, after in (
        ("global palette", PALETTE_OLD, PALETTE_NEW),
        ("bounded scanline copy", COPY_OLD, COPY_NEW),
        ("turbo word copy", TURBO_COPY_OLD, TURBO_COPY_NEW),
    ):
        if after in text:
            continue
        if name == "bounded scanline copy" and text.count(COPY_PREVIOUS) == 1:
            text = text.replace(COPY_PREVIOUS, after, 1)
            continue
        if text.count(before) != 1:
            raise RuntimeError("AnimatedGIF %s patch does not match %s" % (name, source_path))
        text = text.replace(before, after, 1)
    if text != original:
        with open(source_path, "w", encoding="utf-8") as file:
            file.write(text)
        print("Patched AnimatedGIF palette, scanline bounds and alignment-safe word copies")
    return True


def patch_library(library_path):
    header = os.path.join(library_path, "src", "AnimatedGIF.h")
    return _patch_header(header) and _patch_alignment_header(header) and _patch_decoder(
        os.path.join(library_path, "src", "gif.inl")
    )


def patch_animatedgif(env, require_found=False):
    library_path = os.path.join(
        env.subst("$PROJECT_LIBDEPS_DIR"),
        env.subst("$PIOENV"),
        "AnimatedGIF",
    )
    patched_any = os.path.isdir(library_path) and patch_library(library_path)

    if require_found and not patched_any:
        sys.stderr.write("ERROR: AnimatedGIF dependency was not found to patch\n")
        raise SystemExit(1)

    return patched_any


def patch_animatedgif_before_link(source, target, env):
    patch_animatedgif(env, require_found=True)


if "Import" in globals():
    Import("env")  # noqa: F821 (SCons-injected global)
    patch_animatedgif(env, require_found=False)  # noqa: F821
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", patch_animatedgif_before_link)  # noqa: F821
elif __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library_path")
    patch_library(parser.parse_args().library_path)
