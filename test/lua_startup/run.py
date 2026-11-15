#!/usr/bin/env python3
"""Offline API3 regressions bound to production registration, policy and board flags.
No firmware build, provider, generated assets, SDK or shared dependency cache.
"""
import argparse
import configparser
import os
from pathlib import Path
import re
import subprocess
import tempfile

R = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--section', choices=['libraries', 'identity', 'policy', 'all'], default='all')
parser.add_argument('--baseline', help='Read production LuaRunner from this immutable Git revision')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
S = (subprocess.check_output(['git', 'show', args.baseline + ':src/activities/apps/lua/LuaRunner.cpp'], cwd=R, text=True)
     if args.baseline else (R / 'src/activities/apps/lua/LuaRunner.cpp').read_text())

def run(command, **kwargs):
    command = list(map(str, command))
    if args.sanitize and command[0] in ('cc', 'c++'):
        command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command, cwd=R, check=True, **kwargs)

def libraries(out):
    if 'runtime::openEssential(L)' in S:
        header = '#include "LuaRuntime.h"\nusing ink::runtime::openEssential; using ink::runtime::openNetwork;\n'
    else:
        block = re.search(r'  static const luaL_Reg essentialLibs\[\].*?\n  }', S, re.S).group()
        header = 'extern "C" {\n#include "lauxlib.h"\n#include "lualib.h"\n}\nint openEssential(lua_State* L){\n' + block + '\nreturn LUA_OK;}\nint openNetwork(lua_State*,bool){return LUA_OK;}\n'
    (out / 'startup.h').write_text(header)
    flags = ['-O1', '-Ilib/Lua/src', '-Isrc/activities/apps/lua', '-I' + str(out)]
    objects = []
    for source in sorted((R / 'lib/Lua/src').glob('l*.c')):
        obj = out / (source.stem + '.o')
        run(['cc', *flags, '-c', source, '-o', obj])
        objects.append(obj)
    run(['c++', '-std=c++20', *flags, 'test/lua_startup/registration.cpp', *objects, '-lm', '-ldl', '-o', out / 'registration'])
    run([out / 'registration'])

def identity(out):
    cfg = configparser.ConfigParser(interpolation=None, strict=False)
    cfg.read(R / 'platformio.ini')
    def option(section, key):
        if cfg.has_option(section, key):
            value = cfg.get(section, key)
        elif cfg.has_option(section, 'extends'):
            return option(cfg.get(section, 'extends').strip(), key)
        else:
            return ''
        return re.sub(r'\$\{([^}.]+)\.([^}]+)\}', lambda m: option(m[1], m[2]), value)
    body = re.search(r'int LuaRunner::l_getDevice\(lua_State\* L\) \{.*?\n\}', S, re.S).group()
    mapping = re.search(r'#if defined\(FREEINK_DEVICE_X4CLASSIC\).*?#endif', (R / 'include/AppCapabilities.h').read_text(), re.S).group()
    prefix = mapping + '\n#include <cstdio>\nstruct lua_State {};const char* result;void lua_pushstring(lua_State*,const char* s){result=s;}class LuaRunner{public:static int l_getDevice(lua_State*);};\n'
    src = out / 'device.cpp'
    src.write_text(prefix + body + '\nint main(){lua_State L;LuaRunner::l_getDevice(&L);puts(result);}\n')
    cases = []
    for env, expected in [('sticky', 'sticky'), ('sticky-debug', 'sticky'), ('x4-pro', 'x4pro'), ('x4-classic', 'x4'), ('default', 'x3')]:
        defines = re.findall(r'-D((?:FREEINK_DEVICE_|CROSSINK_APP_DEVICE_)[A-Z0-9_]+)(?:=([^\s]+))?', option('env:' + env, 'build_flags'))
        cases.append((env, ['-D' + k + '=' + (v or '1') for k, v in defines], expected))
    cases += [('simulator precedence', ['-DSIMULATOR=1', '-DFREEINK_DEVICE_STICKY=1'], 'simulator'), ('defined-zero', ['-DFREEINK_DEVICE_STICKY=0', '-DFREEINK_DEVICE_X4PRO=0', '-DFREEINK_DEVICE_X4CLASSIC=0'], 'x3')]
    failures = []
    for name, flags, expected in cases:
        run(['c++', *flags, src, '-o', out / 'device'])
        actual = subprocess.check_output([out / 'device'], text=True).strip()
        print(name, 'expected', expected, 'actual', actual, flush=True)
        if actual != expected:
            failures.append(name)
    assert not failures, 'Production board identity: ' + ', '.join(failures)

def policy(out):
    # Assert real startup consumes persisted opt-out instead of the old grant API.
    assert 'http_.domains.count && HalLuaHttp::networkEnabled()' in S, 'production init still uses per-domain grants'
    assert 'runtime::openNetwork(L, http_.domains.count != 0)' in S
    assert 'Invalid network declaration' in S and 'http_.domains = {};' in S
    app = (R / 'src/activities/apps/lua/LuaAppActivity.h').read_text()
    assert 'permissionPending_' not in app and 'runner_->networkEnabled() && !HalLuaHttp::wifiConnected()' in app
    hal = (R / 'lib/hal/HalLuaHttp.cpp').read_text()
    assert 'ink::http::networkEnabled(readOk)' in hal and 'ink::http::setNetworkEnabled(enabled)' in hal
    assignment = re.search(r'http_\.allowed = http_\.domains\.count && HalLuaHttp::networkEnabled\(\);', S)
    assert assignment
    (out / 'admission.h').write_text('#include "AppNetworkPolicy.h"\n#include "HttpCore.h"\nnamespace HalLuaHttp {inline bool networkEnabled(){bool ok;return ink::http::networkEnabled(ok);}}\nstruct Admission {struct {ink::http::Domains domains;bool allowed=false;} http_;void launch(){' + assignment.group() + '}};\n')
    run(['c++', '-std=c++20', '-Itest/lua_startup/stubs', '-Ilib/LuaHttp', '-I' + str(out), 'test/lua_startup/policy.cpp', 'lib/LuaHttp/HttpCore.cpp', '-o', out / 'policy'])
    run([out / 'policy'])
    print('PASS first-run/missing key, opt-out, reenable, manifest-set independence, storage errors and write failures', flush=True)

if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='api3-startup-', dir=os.environ.get('TMPDIR')) as tmp:
        out = Path(tmp)
        for name, fn in [('libraries', libraries), ('identity', identity), ('policy', policy)]:
            if args.section in (name, 'all'):
                print('SECTION', name, flush=True)
                fn(out)
    print('PASS production-bound API3 startup regressions', flush=True)
