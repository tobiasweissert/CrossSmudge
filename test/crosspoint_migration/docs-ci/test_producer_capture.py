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

    def test_post_registration_skips_outer_dummy_graph(self):
        import sys
        sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
        from SCons.Script import Environment
        from unittest.mock import patch
        m = load()
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            env = Environment(tools=['g++'])
            env.Replace(PROJECT_DIR=str(root), BUILD_DIR=str(root/'build'))
            dummy = env.Object(target=str(root/'build/.dummy/sketch.cpp.o'), source=str(root/'.dummy/sketch.cpp'))[0]
            with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')):
                m.install_main(env)
            self.assertFalse(env.File('$BUILD_DIR/src/main.cpp.o').has_builder())
            self.assertEqual(dummy.get_executor().post_actions, [])
            self.assertFalse((root/'evidence').exists())

    def test_opt_in_schedules_only_post_scope_preserving_scripts(self):
        import sys
        sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
        from SCons.Script import Environment
        from unittest.mock import patch
        env = Environment(tools=[])
        env.AddMethod(lambda e,scope:['existing-'+scope], 'GetExtraScripts')
        m = load()
        with patch.dict(os.environ, {}, clear=True):
            m.install(env)
        self.assertEqual(env.GetExtraScripts('post'), ['existing-post'])
        with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE='/unused-synthetic'):
            m.install(env)
        self.assertEqual(env.GetExtraScripts('pre'), ['existing-pre'])
        self.assertEqual(env.GetExtraScripts('post'), ['existing-post', str(HELPER)])

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
            env.Object(target=str(root/'.pio/build/sticky/src/main.cpp.o'), source=str(root/'src/main.cpp'))
            env.AddMethod(lambda self, target, callback: setattr(self, 'action', callback), 'AddPostAction')
            from unittest.mock import patch
            with patch.dict(os.environ, CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')):
                m.install_main(env)
                self.assertEqual(env.action([env.File('src/main.cpp')], [env.File('.pio/build/sticky/src/main.cpp.o')], env), 0)
            receipt = json.loads((root/'evidence/producer-main-receipt.json').read_text())
            self.assertEqual(receipt['probe_exit'], 0)
            self.assertEqual((root/'evidence/producer-application-sdkconfig.h').read_bytes(), header.read_bytes())
            self.assertIn('-DAPP=1', receipt['probe_argv'])
            self.assertFalse(receipt['publication_authorized'])

    def test_real_scons_graph_post_phase_survives_object_lto_executor_replacement(self):
        import sys
        import subprocess
        import textwrap
        scons = Path.home()/'.platformio/packages/tool-scons/scons.py'
        self.assertTrue(scons.is_file(), 'pinned real SCons required; no mocked executor')
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temp:
            root = Path(temp)
            (root/'src').mkdir()
            (root/'src/main.cpp').write_text('// synthetic graph input\n')
            # Actual SCons Object creation and project LTO callback. Only SPAWN
            # is synthetic; neither a compiler nor PlatformIO is launched.
            program = textwrap.dedent('''\
                import os,runpy,shlex
                from pathlib import Path
                import SCons.Platform.posix
                root=Path(Dir('.').get_abspath())
                def no_compiler(shell,escape,cmd,args,execution_env):
                    argv=shlex.split(' '.join(args))
                    p=Path(argv[argv.index('-o')+1]);p.parent.mkdir(parents=True,exist_ok=True)
                    p.write_text('NOT AN OBJECT')
                    return 0
                SCons.Platform.posix.subprocess_spawn=no_compiler
                env=Environment(tools=['g++'],ENV={'PATH':os.defpath})
                env.Replace(SPAWN=no_compiler,PROJECT_DIR=str(root),BUILD_DIR=str(root/'build'),
                    PROJECT_SRC_DIR=str(root/'src'),PIOENV='sticky-owner-test',CCFLAGS=['-Os'],
                    CXX='/nonexistent-python-only-graph-driver')
                env.AddMethod(lambda e,k,default=None: {'custom_sticky_project_lto':'yes',
                    'custom_firmware_device_type':'sticky'}.get(k,default),'GetProjectOption')
                env.AddMethod(lambda e,scope: [],'GetExtraScripts')
                callbacks=[]
                env.AddMethod(lambda e,fn,pattern=None:callbacks.append(fn),'AddBuildMiddleware')
                helper=runpy.run_path(HELPER)
                helper['install'](env)
                runpy.run_path(LTO,init_globals={'env':env,'Import':lambda *a:None})
                # Exact Core CollectBuildFiles -> middleware -> BuildSources
                # ordering: middleware receives source before Object construction.
                # Focused suffix control; full pinned replay separately executes
                # Core's authenticated scons_patched_match_splitext body.
                env.Replace(OBJSUFFIX='.cpp.o')
                env.VariantDir(str(root/'build/src'),str(root/'src'),duplicate=False)
                node=env.File(str(root/'build/src/main.cpp'))
                for fn in callbacks: node=fn(env,node)
                obj=node[0]
                assert obj.get_abspath()==str(root/'build/src/main.cpp.o')
                for script in env.GetExtraScripts('post'):
                    env.SConscript(script,exports={'env':env})
                assert len(obj.get_executor().post_actions)==1
                Default(obj)
                ''')
            # Project SConscript import resolves through PROJECT_DIR, no __file__.
            (root/'scripts').symlink_to(ROOT/'scripts',target_is_directory=True)
            (root/'SConstruct').write_text('HELPER='+repr(str(HELPER))+'\nLTO='+repr(str(ROOT/'scripts/sticky_project_lto_pre.py'))+'\n'+program)
            result = subprocess.run([sys.executable,str(scons),'-Q'],cwd=root,
                env=dict(os.environ,CROSSCUB_PRIVATE_EVIDENCE=str(root/'evidence')),
                capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            receipt=json.loads((root/'evidence/producer-main-receipt.json').read_text())
            self.assertIn('-Oz',receipt['compile_argv'])
            self.assertIn('-flto=1',receipt['compile_argv'])
            self.assertNotIn('-Os',receipt['compile_argv'])
            # No actual diagnostic driver exists in this graph fixture. The
            # original object action succeeded via no-op SPAWN, callback ran,
            # and bounded diagnostic failure remains private/incomplete.
            self.assertIn('probe_error',receipt)
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
        env.Object(target=str(root/'build/src/main.cpp.o'), source=str(root/'src/main.cpp'))
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
            module.install_main(env)
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
            self.assertIn(str(source[0]), replay[0])
            self.assertIn(str(target[0]), replay[0])
            self.assertIs(env['SPAWN'], original_spawn)
            self.assertEqual(dict(env['ENV']), original_env)

    def test_object_local_override_executor_preserves_effective_arguments(self):
        import sys
        sys.path.insert(0, str(Path.home()/'.platformio/packages/tool-scons/scons-local-4.8.1'))
        from SCons.Script import Environment
        env = Environment(tools=['g++'], ENV={'PATH': os.defpath})
        env.Replace(CXXCOM='/not-executed/cxx $CCFLAGS -c $SOURCE -o $TARGET', CCFLAGS=['-Os'])
        obj = env.Object(target='override-main.o', source='override-main.cpp', CCFLAGS=['-flto=1', '-Oz'])[0]
        effective_env = obj.get_executor().get_build_env()
        argv, execution_env = load().effective_compile(effective_env, [obj], obj.sources)
        self.assertIn('-Oz', argv)
        self.assertIn('-flto=1', argv)
        self.assertNotIn('-Os', argv)
        self.assertEqual(env['CCFLAGS'], ['-Os'])
        self.assertEqual(effective_env['CCFLAGS'], ['-flto=1', '-Oz'])
        self.assertEqual(execution_env, dict(effective_env['ENV']))

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
