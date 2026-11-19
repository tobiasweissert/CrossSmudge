import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "patch_animatedgif.py"
SPEC = importlib.util.spec_from_file_location("patch_animatedgif", SCRIPT)
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)

ALIGNMENT_FOOTER = """#if (INTPTR_MAX == INT64_MAX)
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
TURBO_COPY = """        BIGUINT tmp = *(BIGUINT *) s;
        s += sizeof(BIGUINT);
        *(BIGUINT *)d = tmp;
        d += sizeof(BIGUINT);
"""


class Environment:
    def __init__(self, root, name="active"):
        self.root = root
        self.name = name

    def subst(self, value):
        return {"$PROJECT_LIBDEPS_DIR": str(self.root), "$PIOENV": self.name}[value]


class PatchAnimatedGifTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def library(self, name="active"):
        library = self.root / name / "AnimatedGIF"
        source = library / "src"
        source.mkdir(parents=True)
        (source / "AnimatedGIF.h").write_text(PATCH.OLD_SNIPPET + ALIGNMENT_FOOTER)
        (source / "gif.inl").write_text(PATCH.PALETTE_OLD + PATCH.COPY_OLD + TURBO_COPY)
        return library

    def test_active_environment_and_idempotence(self):
        active = self.library()
        sibling = self.library("sibling")
        env = Environment(self.root)
        self.assertTrue(PATCH.patch_animatedgif(env, require_found=True))
        first = {p.name: p.read_bytes() for p in (active / "src").iterdir()}
        self.assertTrue(PATCH.patch_animatedgif(env, require_found=True))
        self.assertEqual(first, {p.name: p.read_bytes() for p in (active / "src").iterdir()})
        self.assertEqual((sibling / "src" / "AnimatedGIF.h").read_text(), PATCH.OLD_SNIPPET + ALIGNMENT_FOOTER)
        self.assertEqual((sibling / "src" / "gif.inl").read_text(), PATCH.PALETTE_OLD + PATCH.COPY_OLD + TURBO_COPY)
        self.assertIn(PATCH.PALETTE_NEW, first["gif.inl"].decode())
        self.assertIn(PATCH.COPY_NEW, first["gif.inl"].decode())

    def test_native_loads_and_word_copies_do_not_dereference_unaligned_types(self):
        library = self.library()
        PATCH.patch_library(library)
        header = (library / 'src/AnimatedGIF.h').read_text()
        decoder = (library / 'src/gif.inl').read_text()
        self.assertNotIn('(*(uint16_t *)p)', header)
        self.assertNotIn('(*(uint32_t *)p)', header)
        self.assertNotIn('(*(uint64_t *)p)', header)
        self.assertNotIn('*(BIGUINT *)', decoder)
        self.assertIn('memcpy', header)

    def test_unknown_decoder_source_fails_without_rewriting_it(self):
        source = self.library() / "src" / "gif.inl"
        source.write_text("unknown upstream implementation")
        with self.assertRaises(RuntimeError):
            PATCH._patch_decoder(source)
        self.assertEqual(source.read_text(), "unknown upstream implementation")

    def test_missing_active_dependency_cannot_be_satisfied_by_sibling(self):
        self.library("sibling")
        self.assertFalse(PATCH.patch_animatedgif(Environment(self.root)))
        with self.assertRaises(SystemExit):
            PATCH.patch_animatedgif(Environment(self.root), require_found=True)


if __name__ == "__main__":
    unittest.main()
