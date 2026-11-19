#!/usr/bin/env python3
"""Run production bidi code and unchanged GoogleTest-style case bodies.
Only the assertion/registration adapter is synthetic; no renderer/SDK claim.
"""
from pathlib import Path
import os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
SHIM=r'''
#pragma once
#include <cassert>
#include <cstdio>
#include <vector>
struct Case { const char* name; void (*run)(); };
inline std::vector<Case>& cases() { static std::vector<Case> c; return c; }
struct Register { Register(const char* n, void (*f)()) { cases().push_back({n,f}); } };
#define TEST(s,n) static void s##_##n(); static Register reg_##s##_##n(#s "." #n,s##_##n); static void s##_##n()
#define EXPECT_EQ(a,b) assert((a)==(b))
#define EXPECT_TRUE(a) assert(a)
#define EXPECT_FALSE(a) assert(!(a))
'''
with tempfile.TemporaryDirectory(prefix='bidi-foundation-',dir=os.environ.get('TMPDIR')) as tmp:
    p=Path(tmp); (p/'gtest').mkdir(); (p/'gtest/gtest.h').write_text(SHIM)
    (p/'Logging.h').write_text('#pragma once\n#define LOG_ERR(...) ((void)0)\n#define LOG_DBG(...) ((void)0)\n')
    (p/'main.cpp').write_text('#include <gtest/gtest.h>\nint main(){for(auto& c:cases()){c.run(); std::printf("PASS %s\\n",c.name);} std::printf("bidi cases: %zu passed\\n",cases().size());}\n')
    subprocess.run(['gcc','-std=c11','-I'+str(ROOT/'lib/MiniBidi'),'-c',str(ROOT/'lib/MiniBidi/minibidi.c'),'-o',str(p/'bidi.o')],check=True,timeout=30)
    subprocess.run(['g++','-std=c++20','-fno-exceptions','-fno-rtti','-I'+str(p),'-I'+str(ROOT/'lib/Utf8'),'-I'+str(ROOT/'lib/Memory'),'-I'+str(ROOT/'lib/MiniBidi'),str(ROOT/'lib/Utf8/Utf8.cpp'),str(ROOT/'lib/MiniBidi/BidiUtils.cpp'),str(ROOT/'test/minibidi_arabic/MiniBidiArabicTest.cpp'),str(p/'main.cpp'),str(p/'bidi.o'),'-o',str(p/'probe')],check=True,timeout=30)
    subprocess.run([str(p/'probe')],check=True,timeout=10)
