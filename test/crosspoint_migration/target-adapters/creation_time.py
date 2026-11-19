#!/usr/bin/env python3
"""Execute the actual FAT birth-time adapter body over an external disk double."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

R = Path(__file__).resolve().parents[3]
GTEST = Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    text = (R / 'lib/hal/HalStorage.cpp').read_text()
    start = text.index('uint32_t HalFile::creationTime()')
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    body = text[start:end]
    source = r'''
#include <gtest/gtest.h>
#include <cstdint>
namespace HalStorage {
struct StorageLock {
  static inline unsigned depth = 0;
  StorageLock() { ++depth; }
  ~StorageLock() { --depth; }
};
}
struct ExternalDiskFile {
  bool available = true;
  uint16_t date = 0x5821, time = 0x1234;
  unsigned calls = 0;
  bool getCreateDateTime(uint16_t* d, uint16_t* t) {
    EXPECT_EQ(HalStorage::StorageLock::depth, 1u);
    ++calls; *d = date; *t = time;
    return available;
  }
};
struct ExternalFileOwner { ExternalDiskFile file; };
struct HalFile {
  ExternalFileOwner* impl;
  uint32_t creationTime();
};
// ACTUAL_BODY
TEST(CreationTime, MissingOwnerStaysUnknown) {
  HalFile f{nullptr}; EXPECT_EQ(f.creationTime(), 0u);
  EXPECT_EQ(HalStorage::StorageLock::depth, 0u);
}
TEST(CreationTime, FailedReadDoesNotInventDate) {
  ExternalFileOwner owner; owner.file.available = false;
  HalFile f{&owner}; EXPECT_EQ(f.creationTime(), 0u);
  EXPECT_EQ(owner.file.calls, 1u);
  EXPECT_EQ(HalStorage::StorageLock::depth, 0u);
}
TEST(CreationTime, ZeroDateStaysUnknown) {
  ExternalFileOwner owner; owner.file.date = 0;
  HalFile f{&owner}; EXPECT_EQ(f.creationTime(), 0u);
}
TEST(CreationTime, ExactFatBitsSurviveRead) {
  ExternalFileOwner owner; HalFile f{&owner};
  EXPECT_EQ(f.creationTime(), (uint32_t{0x5821} << 16) | 0x1234u);
  EXPECT_EQ(owner.file.calls, 1u);
  EXPECT_EQ(HalStorage::StorageLock::depth, 0u);
}
TEST(CreationTime, HighBitPackingDoesNotUseSignedShift) {
  ExternalFileOwner owner; owner.file.date = owner.file.time = 0xffff;
  HalFile f{&owner}; EXPECT_EQ(f.creationTime(), UINT32_MAX);
}
'''.replace('// ACTUAL_BODY', body)
    with tempfile.TemporaryDirectory(prefix='creation-time-', dir=os.environ['TMPDIR']) as temporary:
        directory = Path(temporary)
        probe = directory / 'probe.cpp'
        binary = directory / 'creation.bin'
        probe.write_text(source)
        command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I'+str(GTEST/'include'), '-I'+str(GTEST), str(probe), str(GTEST/'src/gtest-all.cc'), str(GTEST/'src/gtest_main.cc'), '-pthread', '-o', str(binary)]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=120)
        (args.evidence/'compile.log').write_text(compiled.stdout + compiled.stderr)
        receipt = {'command': command, 'compile_exit': compiled.returncode, 'actual_body_sha256': hashlib.sha256(body.encode()).hexdigest(), 'boundary': 'actual creationTime body; external disk and lock doubles; no physical SD acceptance'}
        if compiled.returncode == 0:
            native = subprocess.run([str(binary), '--gtest_output=xml:'+str(args.evidence/'cases.xml')], capture_output=True, text=True, timeout=30)
            (args.evidence/'run.log').write_text(native.stdout + native.stderr)
            receipt.update(native_exit=native.returncode, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
        (args.evidence/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
        print(json.dumps(receipt))
        return receipt.get('native_exit', compiled.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
