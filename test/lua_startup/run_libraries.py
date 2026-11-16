#!/usr/bin/env python3
"""Exercise firmware library startup and NVS policy without captured providers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='owner-libraries-') as temp:
    out = Path(temp)
    baseline = subprocess.check_output(['git', 'show', '528785b5:src/activities/apps/lua/LuaRunner.cpp'], cwd=root, text=True)
    block = re.search(r'  static const luaL_Reg essentialLibs\[\].*?\n  }', baseline, re.S).group()
    (out / 'baseline.h').write_text('inline void baselineEssential(lua_State* L){\n' + block + '\n}\n')
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I' + str(root / 'lib/Lua/src'), '-I' + str(root / 'src/activities/apps/lua'), '-I' + str(out)]
    objects = []
    for source in sorted((root / 'lib/Lua/src').glob('l*.c')):
        obj = out / (source.stem + '.o')
        subprocess.run(['cc', *flags, '-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    for unit, extra in [('libraries', objects + ['-lm', '-ldl']), ('policy', [])]:
        subprocess.run(['c++', '-std=c++20', *flags, '-I' + str(root / 'lib/LuaHttp'), '-I' + str(root / 'test/lua_startup/stubs'), str(root / f'test/lua_startup/{unit}.cpp'), *extra, '-o', str(out / unit)], check=True)
        subprocess.run([str(out / unit)], env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1'), check=True)
