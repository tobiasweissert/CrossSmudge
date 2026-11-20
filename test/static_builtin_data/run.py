#!/usr/bin/env python3
"""Serial standalone entrypoint; host tests do NOT establish PSRAM/startup."""
import fcntl
import os
from pathlib import Path
import subprocess
import sys
import resource

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(os.environ.get('TMPDIR', '/root/.hermes/cache/scratch')) / 'static-builtins-host'


def run(args, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(args, cwd=ROOT, check=True, **kwargs)


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))  # expected assertion control; no dumps
    with open('/root/.hermes/cache/scratch/crosspoint-qualification.lock', 'a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        BUILD.mkdir(exist_ok=True)
        run([sys.executable, 'test/static_builtin_data/test_generator.py', '-v'])
        run([sys.executable, 'test/static_builtin_data/test_integration_policy.py'])
        if '--proposal' in sys.argv:
            proposal = sys.argv[sys.argv.index('--proposal') + 1]
            run([sys.executable, 'test/static_builtin_data/test_proposal.py', proposal])
        assert (ROOT / 'lib/StaticBuiltinData/StaticBuiltinData.h').exists(), 'fixed hydration subsystem missing'
        sanitize = '--sanitize' in sys.argv
        flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitize else []
        common = ['-g0', '-O1', *flags, '-DSTATIC_BUILTIN_HOST_TEST=1',
                  '-Ilib/StaticBuiltinData', '-Ilib/EpdFont', '-Ilib/miniz/src']
        for identity, options, expected in [
            ('disabled_opt_in', [], 'opt-in only'),
            ('packed_abi', ['-DSTATIC_BUILTIN_HOST_TEST=1', '-fpack-struct=1'], 'glyph ABI'),
            ('big_endian', ['-DSTATIC_BUILTIN_HOST_TEST=1', '-U__BYTE_ORDER__', '-D__BYTE_ORDER__=__ORDER_BIG_ENDIAN__'], 'unsupported endian'),
        ]:
            result = subprocess.run(['c++', '-std=c++17', '-fsyntax-only', '-x', 'c++',
                                     '-Ilib/StaticBuiltinData', '-Ilib/EpdFont', '-Ilib/miniz/src', *options, '-'],
                                    input='#include "StaticBuiltinData.h"\n', cwd=ROOT, capture_output=True, text=True)
            assert result.returncode != 0 and expected in result.stderr, (identity, result.stderr)
            print('PASS negative_compile_'+identity, flush=True)
        abi = BUILD/'negative-abi'
        abi.mkdir(exist_ok=True)
        original = (ROOT/'lib/EpdFont/EpdFontData.h').read_text()
        assert original.count('uint32_t dataOffset;') == 1
        (abi/'EpdFontData.h').write_text(original.replace('uint32_t dataOffset;', 'float dataOffset;'))
        result = subprocess.run(['c++', '-std=c++17', '-fsyntax-only', '-x', 'c++',
                                 '-DSTATIC_BUILTIN_HOST_TEST=1', '-I'+str(abi),
                                 '-Ilib/StaticBuiltinData', '-Ilib/miniz/src', '-'],
                                input='#include "StaticBuiltinData.h"\n', cwd=ROOT, capture_output=True, text=True)
        assert result.returncode != 0 and 'glyph field types' in result.stderr, 'same-layout nonscalar ABI was not rejected: '+result.stderr
        print('PASS negative_compile_same_layout_wrong_field_type', flush=True)
        run(['cc', *common, '-c', 'lib/miniz/src/miniz_impl.c', '-o', str(BUILD/'miniz.o')])
        run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *common,
             'test/static_builtin_data/HydrationTest.cpp', 'lib/StaticBuiltinData/StaticBuiltinData.cpp',
             'test/static_builtin_data/AllocationObserver.cpp', '-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free',
             str(BUILD/'miniz.o'), '-o', str(BUILD/'hydrate')])
        control = subprocess.run([str(BUILD/'hydrate'), '--allocation-negative'], cwd=ROOT, capture_output=True, text=True)
        assert control.returncode != 0 and '!forbidAllocation' in control.stderr, control.stderr
        print('PASS allocation_observer_fires_negative_control', flush=True)
        run([str(BUILD/'hydrate')], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
                                       'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
        run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *common,
             'test/static_builtin_data/BitmapSemanticsTest.cpp', 'lib/StaticBuiltinData/StaticBuiltinData.cpp',
             'test/static_builtin_data/AllocationObserver.cpp', '-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free',
             str(BUILD/'miniz.o'), '-o', str(BUILD/'bitmap')])
        run([str(BUILD/'bitmap')], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
                                      'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
        run([sys.executable, 'scripts/gen_static_builtin_data.py', '--out', str(BUILD/'generated'), '--profile', 'esp32-le-v1'])
        generated = BUILD/'generated'
        reference = BUILD/'raw-reference'
        reference.mkdir(exist_ok=True)
        run([sys.executable, 'scripts/gen_i18n.py', 'lib/I18n/translations', str(reference),
             '--strip-unused', '--src-dirs', 'src', 'lib'])
        for filename in ['I18nKeys.h', 'I18nStrings.h', 'I18nStrings.cpp']:
            outputs = []
            for directory in [reference, generated]:
                result = subprocess.run(['c++', '-std=c++17', '-E', '-P', '-x', 'c++',
                                         '-I'+str(directory), '-'],
                                        input=f'#include "{filename}"\n'.encode(),
                                        cwd=ROOT, check=True, capture_output=True)
                assert not result.stderr, result.stderr.decode()
                outputs.append(result.stdout)
            assert outputs[0] == outputs[1], 'ordinary raw preprocessed output changed: '+filename
        print('PASS ordinary_raw_preprocessed_headers_offsets_metadata_cpp_identical', flush=True)
        includes = ['-I'+str(generated), '-Ilib/I18n', '-Itest/static_builtin_data', '-Ilib/Utf8']
        observer = ['test/static_builtin_data/AllocationObserver.cpp',
                    '-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free']
        # Compile the unchanged lookup implementation against the raw alternative,
        # then against all fixed backing; compare every byte emitted by the API.
        base = ['c++', '-std=c++17', '-g0', '-O1', *flags, *includes]
        sources = ['test/static_builtin_data/IntegrationTest.cpp', 'lib/I18n/I18n.cpp', str(generated/'I18nStrings.cpp')]
        raw_sources = ['test/static_builtin_data/IntegrationTest.cpp', 'lib/I18n/I18n.cpp', str(reference/'I18nStrings.cpp')]
        run([base[0], '-I'+str(reference), *base[1:], *raw_sources, '-o', str(BUILD/'raw')])
        run([*base, *common, '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
             *sources, *observer, 'lib/EpdFont/EpdFont.cpp', 'lib/Utf8/Utf8.cpp',
             'lib/StaticBuiltinData/StaticBuiltinData.cpp', str(generated/'StaticBuiltinFonts.cpp'),
             str(generated/'StaticBuiltinOwner.cpp'), str(BUILD/'miniz.o'), '-o', str(BUILD/'packed')])
        for name in ['raw', 'packed']:
            with (BUILD/(name+'.txt')).open('wb') as output:
                run([str(BUILD/name)], stdout=output,
                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        assert (BUILD/'raw.txt').read_bytes() == (BUILD/'packed.txt').read_bytes()
        print('PASS raw_vs_packed_every_language_strid_metadata', flush=True)


if __name__ == '__main__':
    main()
