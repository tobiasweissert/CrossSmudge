"""Producer-boundary preparation only: never runs a real compiler/PlatformIO."""
import ast
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / 'scripts/ci_producer_capture.py'
PRODUCER = Path.home()/'.platformio/platforms/espressif32/builder/frameworks/espidf.py'
if not PRODUCER.is_file():
    PRODUCER = Path('/root/.hermes/task-state/crosscub-owner-next-release/plugin-hub/target-44834357235/selected/startup-builder-espidf.py')


def load():
    spec = importlib.util.spec_from_file_location('producer_capture', HELPER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ProducerTests(unittest.TestCase):
    def test_real_producer_preserves_before_cleanup_and_forwards_verbose(self):
        self.assertTrue(HELPER.exists(), 'missing producer-boundary implementation')
        if not PRODUCER.exists():
            self.skipTest('authenticated producer source available only in preparation workspace')
        m = load()
        original = PRODUCER.read_bytes()
        patched = m.overlay(original)
        self.assertEqual(m.overlay(patched), patched)
        tree = ast.parse(patched)
        function = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == 'idf_lib_copy')
        # Actual producer body, fake external build only; no invented producer model.
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            build = root / '.pio/build/sticky'
            (build / 'config').mkdir(parents=True)
            (build / 'config/sdkconfig.h').write_text('#define CONFIG_SPIRAM 1\n')
            (build / 'config/sdkconfig.json').write_text('{}')
            (root / 'sdkconfig.sticky').write_text('CONFIG_SPIRAM=y\n')
            libs = root / 'libs'
            (libs / 'esp32s3').mkdir(parents=True)
            (libs / 'esp32s3/sdkconfig').write_text('old')
            calls = []
            class Env(dict):
                def subst(self, value):
                    return {'$PROJECT_DIR': str(root), '$PYTHONEXE': '/fake/python'}[value]
                def Execute(self, command):
                    calls.append(command)
                    self_test.assertTrue((root/'evidence/producer-idf-sdkconfig.h').is_file())
                    self_test.assertFalse((build/'config').exists())
                    return 0
                def VerboseAction(self, command, label):
                    return command
                def Exit(self, code):
                    calls.append(code)
            self_test = self
            env = Env(PROJECT_BUILD_DIR=str(root/'.pio/build'), PIOENV='sticky')
            import shutil
            namespace = dict(Path=Path, shutil=shutil, os=os, ARDUINO_FRMWRK_LIB_DIR=str(libs),
                             chip_variant='esp32s3', board={}, sdk_config={},
                             copy_idf_component_archives=lambda *a: None, IS_WINDOWS=False,
                             get_requested_cli_targets=lambda: [], ARGUMENTS={}, idf_variant='esp32s3',
                             flag_custom_component_add=False, flag_custom_component_remove=False)
            exec(compile(ast.Module(body=[function], type_ignores=[]), '<actual-producer>', 'exec'), namespace)
            from unittest.mock import patch
            with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence'), CROSSCUB_PROJECT_SCRIPTS=str(ROOT/'scripts')):
                namespace['idf_lib_copy']([], [], env)
            self.assertIn('run --verbose -e "sticky"', calls[0])
            self.assertEqual((root/'evidence/producer-idf-sdkconfig.h').read_bytes(), b'#define CONFIG_SPIRAM 1\n')
            self.assertEqual(calls[-1], 0)

    def test_middleware_selects_actual_variant_source_not_dummy(self):
        self.assertTrue(HELPER.exists(), 'missing producer-boundary implementation')
        m = load()
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            class Node:
                def __init__(self, path): self.path = path
                def get_abspath(self): return str(root/'.pio/build/sticky'/self.path)
                def srcnode(self):
                    from types import SimpleNamespace
                    return SimpleNamespace(get_abspath=lambda: str(root/self.path))
            class Env:
                def subst(self, value): return str(root) if value == '$PROJECT_DIR' else str(root/'.pio/build/sticky')
                def AddBuildMiddleware(self, callback, pattern): self.callback = callback
                def AddPostAction(self, target, callback): self.action = callback
            from unittest.mock import patch
            env = Env()
            with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')):
                m.install(env)
                dummy = Node('.dummy/sketch.cpp')
                self.assertIs(env.callback(env, dummy), dummy)
                self.assertFalse(hasattr(env, 'action'))
                node = Node('src/main.cpp')
                self.assertIs(env.callback(env, node), node)
                self.assertTrue(hasattr(env, 'action'), 'variant srcnode must install actual application post action')

    def test_actual_main_callback_runs_fake_external_preprocessor(self):
        self.assertTrue(HELPER.exists(), 'missing producer-boundary implementation')
        m = load()
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            (root/'src').mkdir()
            (root/'src/main.cpp').write_text('// synthetic main input\n')
            header = root/'sdk/include/sdkconfig.h'
            header.parent.mkdir(parents=True)
            header.write_text('#define CONFIG_SPIRAM 1\n')
            fake = root/'fake-driver'
            import sys
            fake.write_text('#!'+sys.executable+'\nimport sys,pathlib\na=sys.argv\n'
                'assert "-c" not in a and "-MD" in a and "-E" in a\n'
                'pathlib.Path(a[a.index("-MF")+1]).write_text(a[a.index("-MT")+1]+": src/main.cpp '+str(header)+'\\n")\n'
                'pathlib.Path(a[a.index("-o")+1]).write_text("#define CONFIG_SPIRAM 1\\n")\n')
            fake.chmod(0o700)
            class Node:
                def srcnode(self): return self
                def get_abspath(self): return str(root/'src/main.cpp')
            import sys
            sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
            from SCons.Script import Environment
            env = Environment(tools=['g++'], ENV={'PATH': os.defpath})
            env.Replace(PIOENV='sticky', PROJECT_DIR=str(root), BUILD_DIR=str(root/'.pio/build/sticky'),
                        CXX=str(fake), CXXCOM='$CXX -DAPP=1 -c $SOURCE -o $TARGET')
            env.AddMethod(lambda self, callback, pattern: setattr(self, 'callback', callback), 'AddBuildMiddleware')
            env.AddMethod(lambda self, target, callback: setattr(self, 'action', callback), 'AddPostAction')
            from unittest.mock import patch
            with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')):
                m.install(env)
                env.callback(env, Node())
                self.assertEqual(env.action([env.File('src/main.cpp')], [env.File('.pio/build/sticky/src/main.cpp.o')], env), 0)
            receipt = json.loads((root/'evidence/producer-main-receipt.json').read_text())
            self.assertEqual(receipt['probe_exit'], 0)
            self.assertEqual((root/'evidence/producer-application-sdkconfig.h').read_bytes(), header.read_bytes())
            self.assertIn('-DAPP=1', receipt['probe_argv'])
            self.assertFalse(receipt['publication_authorized'])

    def test_overlay_rejects_unknown_bytes(self):
        self.assertTrue(HELPER.exists(), 'missing producer-boundary implementation')
        with self.assertRaises(ValueError):
            load().overlay(b'unknown producer')

    def test_probe_uses_exact_compile_arguments_without_compiling(self):
        self.assertTrue(HELPER.exists(), 'missing producer-boundary implementation')
        args = ['/fake/g++', '-o', 'main.o', '-c', '-MMD', '-MF', 'main.d', '-DVALUE=2', '-isystem', '/sdk', 'src/main.cpp']
        command = load().probe_command(args, 'proof.d', 'proof.macros', 'main.o')
        self.assertEqual(command, ['/fake/g++', '-DVALUE=2', '-isystem', '/sdk', 'src/main.cpp', '-E', '-dM', '-MD', '-MF', 'proof.d', '-MT', 'main.o', '-o', 'proof.macros'])
        self.assertEqual(args[1:3], ['-o', 'main.o'])

class BoundedProducerTests(unittest.TestCase):
    def invoke(self, root, body, deadline=0.25, inject_exception=False):
        import sys
        import shlex
        from unittest.mock import patch
        sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
        from SCons.Script import Environment
        (root/'src').mkdir(exist_ok=True)
        (root/'src/main.cpp').write_text('// fixture')
        driver = root/'python driver.py'
        driver.write_text('import sys,os,pathlib,time,subprocess\n'
                          'assert os.environ["ONLY_CONSTRUCTION"] == "synthetic-env-sentinel"\n'
                          'assert \'-DCROSSCUB_APP_VERSION="release"\' in sys.argv\n'+body)
        env = Environment(tools=['g++'], ENV={'PATH': os.defpath, 'ONLY_CONSTRUCTION': 'synthetic-env-sentinel'})
        env.Replace(PIOENV='sticky', PROJECT_DIR=str(root), BUILD_DIR=str(root/'build'),
                    CXXCOM=shlex.quote(sys.executable)+' '+shlex.quote(str(driver))+r' -DCROSSCUB_APP_VERSION=\"release\" -c $SOURCE -o $TARGET')
        env.AddMethod(lambda self, fn, pattern: setattr(self, 'callback', fn), 'AddBuildMiddleware')
        env.AddMethod(lambda self, target, fn: setattr(self, 'action', fn), 'AddPostAction')
        module = load()
        import selectors
        select = selectors.DefaultSelector.select
        def fault(selector, timeout=None):
            if inject_exception and (root/'pid').exists():
                raise OSError('synthetic-monitor-failure')
            return select(selector, timeout)
        # Public callback, real SCons expansion and Python-only driver.
        with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')), patch.object(module, 'PROBE_TIMEOUT', deadline, create=True), patch.object(selectors.DefaultSelector, 'select', fault):
            module.install(env)
            env.callback(env, env.File(str(root/'src/main.cpp')))
            self.assertEqual(env.action([env.File(str(root/'src/main.cpp'))], [env.File(str(root/'build/main.o'))], env), 0)
        receipt = json.loads((root/'evidence/producer-main-receipt.json').read_text())
        expected_env_digest = hashlib.sha256(json.dumps(dict(env['ENV']), sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        self.assertEqual(receipt['compile_env_sha256'], expected_env_digest)
        for path in (root/'evidence').iterdir():
            self.assertNotIn(b'synthetic-env-sentinel', path.read_bytes(), 'environment value leaked into evidence')
            self.assertNotIn(b'ONLY_CONSTRUCTION', path.read_bytes(), 'environment dump leaked into evidence')
        return receipt

    def test_each_diagnostic_output_is_bounded(self):
        for channel in ('stdout', 'stderr', 'dependency', 'macro'):
            with self.subTest(channel=channel), tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
                root = Path(temp)
                body = {'stdout': 'os.write(1,b"x"*300000)', 'stderr': 'os.write(2,b"x"*300000)',
                        'dependency': 'pathlib.Path(sys.argv[sys.argv.index("-MF")+1]).write_bytes(b"x"*300000)',
                        'macro': 'pathlib.Path(sys.argv[sys.argv.index("-o")+1]).write_bytes(b"x"*300000)'}[channel]
                receipt = self.invoke(root, body)
                self.assertEqual(receipt.get('probe_error'), 'OutputLimitExceeded')
                self.assertNotEqual(receipt.get('probe_exit'), 0)
                for path in (root/'evidence').iterdir():
                    self.assertLessEqual(path.stat().st_size, 262144, path.name)
                self.assertNotIn('producer-main.macros_sha256', receipt)

    def test_descendants_cannot_write_after_callback(self):
        import time
        import sys
        for mode in ('timeout', 'normal', 'closed-pipes', 'overflow', 'exception'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
                root = Path(temp)
                sentinel = root/'late-write'
                pidfile = root/'pid'
                child = 'import pathlib,time;time.sleep(0.65);pathlib.Path('+repr(str(sentinel))+').write_text("late")'
                body = 'p=subprocess.Popen([sys.executable,"-c",'+repr(child)+'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)\npathlib.Path('+repr(str(pidfile))+').write_text(str(p.pid))\n'
                if mode == 'closed-pipes':
                    body += 'os.close(1);os.close(2)\n'
                if mode == 'overflow':
                    body += 'os.write(2,b"x"*300000)\n'
                if mode != 'normal':
                    body += 'time.sleep(5)\n'
                receipt = self.invoke(root, body, inject_exception=mode == 'exception')
                expected_error = {'timeout': 'TimeoutExpired', 'closed-pipes': 'TimeoutExpired',
                                  'overflow': 'OutputLimitExceeded', 'exception': 'OSError'}
                if mode in expected_error:
                    self.assertEqual(receipt.get('probe_error'), expected_error[mode])
                status = Path('/proc')/pidfile.read_text()/'stat'
                self.assertTrue(not status.exists() or status.read_text().split()[2] == 'Z', 'live descendant at callback return')
                time.sleep(0.8)
                self.assertFalse(sentinel.exists(), 'descendant wrote after callback returned')
                self.assertFalse(status.exists(), 'descendant not reaped by host init')

class EffectiveArgvTests(unittest.TestCase):
    def test_real_scons_spawn_semantics_no_compiler(self):
        import sys
        import shlex
        import subprocess
        sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
        import SCons
        from SCons.Script import Environment, Action
        self.assertEqual(SCons.__version__, '4.8.1')
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            # No compiler: the real SCons SPAWN boundary is intercepted, then
            # its POSIX shell interpretation is checked with a Python argv echo.
            echo = root/"echo driver's with spaces.py"
            echo.write_text('import sys,json,os; print(json.dumps([sys.argv[1:],os.getenv("ONLY_CONSTRUCTION")]))')
            env = Environment(tools=['g++'], ENV={'PATH': os.defpath, 'ONLY_CONSTRUCTION': 'synthetic-env-sentinel'})
            env.Replace(CXXCOM=shlex.quote(sys.executable)+' '+shlex.quote(str(echo))+
                        r''' -DCROSSCUB_APP_VERSION=\"release\" -I"include with spaces" -DNAME='quoted value' $SOURCE -o $TARGET''')
            source = [env.File(str(root/'main input.cpp'))]
            target = [env.File(str(root/'main output.o'))]
            seen = []
            def spawn(shell, escape, cmd, args, execution_env):
                seen.append((args, dict(execution_env)))
                return 0
            oracle = env.Clone(SPAWN=spawn)
            self.assertEqual(Action('$CXXCOM').execute(target, source, oracle), 0)
            raw, execution_env = seen[0]
            actual = json.loads(subprocess.check_output([env['SHELL'], '-c', ' '.join(raw)], env=execution_env))
            original_spawn = env['SPAWN']
            original_env = dict(env['ENV'])
            m = load()
            argv, effective_env = m.effective_compile(env, target, source)
            replay = json.loads(subprocess.check_output(argv, env=effective_env))
            self.assertEqual(replay, actual)
            self.assertIn('-DCROSSCUB_APP_VERSION="release"', replay[0])
            self.assertIn('-Iinclude with spaces', replay[0])
            self.assertIn('-DNAME=quoted value', replay[0])
            self.assertEqual(replay[1], 'synthetic-env-sentinel')
            self.assertIn(str(root/'main input.cpp'), replay[0])
            self.assertIn(str(root/'main output.o'), replay[0])
            self.assertIs(env['SPAWN'], original_spawn)
            self.assertEqual(dict(env['ENV']), original_env)

    def test_shell_expansion_and_unknown_spawn_fail_closed_without_execution(self):
        import sys
        sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
        from SCons.Script import Environment
        env = Environment(tools=['g++'])
        module = load()
        for command in ('/fake/cxx *.cpp', '/fake/cxx ; echo bad', '/fake/cxx `id`', '/fake/cxx ~root'):
            env.Replace(CXXCOM=command)
            with self.subTest(command=command), self.assertRaises(ValueError):
                module.effective_compile(env, [env.File('output.o')], [env.File('input.cpp')])
        env.Replace(SPAWN=lambda *args: self.fail('unknown spawn executed'))
        with self.assertRaises(ValueError):
            module.effective_compile(env, [], [])

if __name__ == '__main__':
    unittest.main()
