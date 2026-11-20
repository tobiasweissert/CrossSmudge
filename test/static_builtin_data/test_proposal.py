"""Exercise an UNAPPLIED patch in a small plain-file overlay, never a worktree.

Config inheritance and bridge guard AST are host policy evidence, not resolved
SDK configuration or target startup/SCons graph proof.
"""
import ast
import configparser
import importlib.util
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
EXPERIMENT = 'sticky-static-builtins-experimental'


def config(path):
    parser = configparser.ConfigParser(interpolation=None)
    parser.read(path)
    return parser


def option(parser, section, key, default=''):
    if key in parser[section]:
        value = parser[section][key]
    elif 'extends' in parser[section]:
        value = option(parser, parser[section]['extends'], key, default)
    else:
        value = default
    return re.sub(r'\$\{([^}]+)\}', lambda m: option(parser, *m[1].rsplit('.', 1)), value)


class Proposal(unittest.TestCase):
    def test_existing_sections_and_default_selection_unchanged(self):
        for section in BEFORE.sections():
            with self.subTest(section=section):
                self.assertEqual(dict(BEFORE[section]), dict(AFTER[section]))
        self.assertEqual(AFTER['platformio']['default_envs'], 'sticky')
        self.assertEqual(set(AFTER.sections()) - set(BEFORE.sections()), {'env:'+EXPERIMENT})

    def test_opt_in_only_separate_experiment(self):
        aliases = ['sticky', 'default', 'sticky-owner-test', 'sticky-debug', 'debug', 'sticky-simulator', 'simulator']
        for name in aliases:
            with self.subTest(name=name):
                self.assertEqual(option(AFTER, 'env:'+name, 'custom_static_builtin_data', 'no'), 'no')
        self.assertIn('env:'+EXPERIMENT, AFTER)
        self.assertEqual(option(AFTER, 'env:'+EXPERIMENT, 'custom_static_builtin_data', 'no'), 'yes')
        for key in ['custom_sdkconfig', 'build_flags', 'build_unflags', 'extra_scripts',
                    'board_build.partitions', 'board_upload.maximum_size', 'platform',
                    'custom_sticky_project_lto', 'custom_firmware_device_type', 'custom_i18n_builtin_langs']:
            with self.subTest(inherited=key):
                self.assertEqual(option(AFTER, 'env:'+EXPERIMENT, key), option(BEFORE, 'env:sticky', key))

    def test_bridge_guard_exact_environment_and_all_languages(self):
        tree = ast.parse((OVERLAY/'scripts/gen_i18n.py').read_text())
        bridges = [node for node in ast.walk(tree) if isinstance(node, ast.If)
                   and isinstance(node.test, ast.BoolOp) and 'custom_static_builtin_data' in ast.unparse(node.test)]
        self.assertEqual(len(bridges), 1)
        bridge = bridges[0]
        # Execute the real opt-in predicate and prerequisite guards only. The
        # generator/install side effects are deliberately outside this AST test.
        guards = []
        for node in bridge.body:
            if not isinstance(node, ast.If):
                break
            guards.append(node)
        self.assertEqual(len(guards), 2)
        bridge.body = guards + [ast.Assign(targets=[ast.Name(id='accepted', ctx=ast.Store())], value=ast.Constant(True))]
        code = compile(ast.fix_missing_locations(ast.Module(body=[bridge], type_ignores=[])), '<proposal-guards>', 'exec')
        class Env(dict):
            def GetProjectOption(self, key, default):
                return self.get(key, default)
        for name in [EXPERIMENT, 'sticky', 'default', 'sticky-owner-test', 'sticky-debug', 'debug', 'sticky-simulator', 'simulator']:
            for optin in ['no', 'yes']:
                for langs in ['all', '', 'en']:
                    env = Env(PIOENV=name, custom_static_builtin_data=optin, custom_i18n_builtin_langs=langs)
                    scope = dict(env=env, Import=lambda *_: None, accepted=False)
                    rejected = optin == 'yes' and (name != EXPERIMENT or langs == 'en')
                    with self.subTest(name=name, optin=optin, langs=langs):
                        if rejected:
                            with self.assertRaises(RuntimeError):
                                exec(code, scope)
                        else:
                            error = None
                            try:
                                exec(code, scope)
                            except RuntimeError as exc:
                                error = str(exc)
                            self.assertIsNone(error, 'unexpected guard rejection: '+str(error))
                            self.assertEqual(scope['accepted'], optin == 'yes')

    def test_experiment_actual_lto_hooks_keep_sdk_and_object_policy(self):
        self.assertIn('env:'+EXPERIMENT, AFTER)
        env = LTO.Environment()
        env['PIOENV'] = EXPERIMENT
        env.GetProjectOption = lambda key, default=None: option(AFTER, 'env:'+EXPERIMENT, key, default)
        before = dict(env)
        for script in ['sticky_project_lto_pre.py', 'sticky_project_lto_post.py']:
            runpy.run_path(str(OVERLAY/'scripts'/script), init_globals=dict(env=env, Import=lambda *_: None))
        self.assertIsNotNone(env.callback)
        self.assertEqual(env['CCFLAGS'], before['CCFLAGS'])
        self.assertEqual(env['CFLAGS'], before['CFLAGS'])
        self.assertEqual(env['CXXFLAGS'], before['CXXFLAGS'])
        self.assertEqual(env['LINKFLAGS'], ['-Wl,--gc-sections', '-flto=1', '-fuse-linker-plugin', '-Oz'])
        for path in ['/fixture/project/src/main.cpp', '/fixture/project/.pio/libdeps/'+EXPERIMENT+'/pkg/a.c']:
            env.callback(env, LTO.Node(path))
            self.assertEqual(env.objects[-1][1]['CCFLAGS'], ['-flto=1', '-Oz'])
        count = len(env.objects)
        for path in ['/sdk/framework-espidf/tasks.c', '/fixture/project/.pio/build/'+EXPERIMENT+'/esp-idf/a.c', '/fixture/project/src/start.S']:
            node = LTO.Node(path)
            self.assertIs(env.callback(env, node), node)
        self.assertEqual(len(env.objects), count)

    def test_first_setup_fence_and_single_owner_include(self):
        main = (OVERLAY/'src/main.cpp').read_text()
        setup = main.split('void setup() {', 1)[1]
        self.assertTrue(setup.startswith('\n#if defined(CROSSPOINT_STATIC_BUILTINS)'))
        fence = setup.index('if (!static_builtin::hydrateBuiltins()) std::abort();')
        self.assertLess(fence, setup.index('SimulatorLifecycle::'))
        core = (OVERLAY/'lib/StaticBuiltinData/StaticBuiltinData.cpp').read_text()
        self.assertEqual(core.count('#include "StaticBuiltinOwner.cpp"'), 1)
        self.assertEqual(core.count('#include "StaticBuiltinFonts.cpp"'), 1)


if __name__ == '__main__':
    proposal = Path(sys.argv[1]).resolve()
    BEFORE = config(ROOT/'platformio.ini')
    scratch = proposal.parent.parent/'scratch'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='static-builtins-proposal-', dir=scratch) as directory:
        OVERLAY = Path(directory)
        paths = ['platformio.ini', 'src/main.cpp', 'lib/StaticBuiltinData/StaticBuiltinData.cpp',
                 'scripts/gen_i18n.py', 'scripts/sticky_project_lto_pre.py', 'scripts/sticky_project_lto_post.py',
                 'test/crosspoint_migration/docs-ci/test_project_lto.py']
        for name in paths:
            target = OVERLAY/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT/name, target)
        applied = subprocess.run(['patch', '--batch', '--fuzz=0', '-p1', '-i', str(proposal)], cwd=OVERLAY,
                                 capture_output=True, text=True)
        print(applied.stdout + applied.stderr, flush=True)
        if applied.returncode:
            sys.exit(applied.returncode)
        AFTER = config(OVERLAY/'platformio.ini')
        spec = importlib.util.spec_from_file_location('baseline_lto', OVERLAY/paths[-1])
        LTO = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(LTO)
        suite = unittest.TestSuite([unittest.defaultTestLoader.loadTestsFromTestCase(Proposal),
                                    unittest.defaultTestLoader.loadTestsFromTestCase(LTO.ProjectLtoPolicy)])
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        sys.exit(not result.wasSuccessful())
