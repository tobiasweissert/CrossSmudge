#!/usr/bin/env python3
"""Project-only LTO policy checks; no target compiler or SDK changes."""
from pathlib import Path
import configparser
import copy
import re
import runpy
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class Node:
    def __init__(self, path): self.path = path
    def srcnode(self): return self
    def get_abspath(self): return self.path


class Environment(dict):
    def __init__(self, enabled=True, device='sticky'):
        super().__init__(CCFLAGS=['-Os', '-fno-lto'], CFLAGS=['-fno-lto'], CXXFLAGS=['-fno-lto', '-fno-exceptions'], LINKFLAGS=['-fno-lto', '-Wl,--gc-sections'])
        self.enabled, self.device, self.callback, self.objects = enabled, device, None, []
    def GetProjectOption(self, name, default=None):
        return {'custom_sticky_project_lto': 'yes' if self.enabled else 'no', 'custom_firmware_device_type': self.device}.get(name, default)
    def subst(self, name):
        return {'$PROJECT_DIR': '/fixture/project', '$PROJECT_SRC_DIR': '/fixture/project/src', '$PROJECT_LIB_DIR': '/fixture/project/lib', '$PROJECT_LIBDEPS_DIR': '/fixture/project/.pio/libdeps'}.get(name, name)
    def AddBuildMiddleware(self, callback): self.callback = callback
    def Object(self, node, **flags):
        self.objects.append((node.path, flags))
        return ('compiled-object', node.path)
    def Replace(self, **values): self.update(values)


class ProjectLtoPolicy(unittest.TestCase):
    def load(self, filename, env):
        path = ROOT/'scripts'/filename
        self.assertTrue(path.is_file(), 'missing project-only LTO hook')
        return runpy.run_path(str(path), init_globals={'Import': lambda *args: None, 'env': env})

    def test_selected_objects_have_only_oz_without_parent_mutation(self):
        # Real hook at the pinned Core's callback(env, variant_source_node)
        # boundary; Object records keyword overrides, not a SCons graph proof.
        parent = Environment()
        self.load('sticky_project_lto_pre.py', parent)
        object_env = Environment()
        object_env.update(
            CCFLAGS=['-Os', '-O2', '-flto=auto', '-fno-rtti', '-DPROTECTED=1'],
            CFLAGS=['-Og', '-O', '-fno-lto', '-std=gnu17'],
            CXXFLAGS=['-Ofast', '-O0', '-Oz', '-O3', '-flto',
                      '-fno-exceptions', '-std=gnu++17'])
        before = copy.deepcopy(dict(object_env))
        parent_before = copy.deepcopy(dict(parent))
        for path in ['/fixture/project/src/main.cpp',
                     '/fixture/project/lib/math/helper.c',
                     '/fixture/project/.pio/libdeps/sticky/pkg/helper.cc',
                     '/fixture/project/freeink-sdk/libs/ui/helper.cxx']:
            with self.subTest(path=path):
                source = Node(path)
                variant = Node('/fixture/project/.pio/build/sticky/variant.cpp')
                variant.srcnode = lambda: source
                parent.callback(object_env, variant)
                self.assertFalse(parent.objects)
                self.assertEqual(object_env.objects[-1][0], variant.path)
                flags = object_env.objects[-1][1]
                for language in ('CFLAGS', 'CXXFLAGS'):
                    command_flags = flags['CCFLAGS'] + flags[language]
                    optimizations = [f for f in command_flags
                                     if re.fullmatch(r'-O(?:[0-9]+|s|z|g|fast)?', f)]
                    self.assertEqual(optimizations, ['-Oz'])
                    self.assertEqual([f for f in command_flags
                                      if f == '-flto' or f.startswith('-flto=')], ['-flto=1'])
                    self.assertNotIn('-fno-lto', command_flags)
                self.assertIn('-fno-rtti', flags['CCFLAGS'])
                self.assertIn('-DPROTECTED=1', flags['CCFLAGS'])
                self.assertIn('-std=gnu17', flags['CFLAGS'])
                self.assertIn('-std=gnu++17', flags['CXXFLAGS'])
                self.assertIn('-fno-exceptions', flags['CXXFLAGS'])
                self.assertEqual(flags, {
                    'CCFLAGS': ['-fno-rtti', '-DPROTECTED=1', '-flto=1', '-Oz'],
                    'CFLAGS': ['-std=gnu17'],
                    'CXXFLAGS': ['-fno-exceptions', '-std=gnu++17']})
                self.assertEqual(dict(object_env), before)
                self.assertEqual(dict(parent), parent_before)

    def test_only_project_c_cpp_sources_are_optimized(self):
        env = Environment(); self.load('sticky_project_lto_pre.py', env)
        self.assertIsNotNone(env.callback)
        for path in ['/fixture/project/src/main.cpp', '/fixture/project/lib/Lua/src/lapi.c', '/fixture/project/freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp', '/fixture/project/.pio/libdeps/sticky/PNGdec/PNGdec.cpp']:
            with self.subTest(path=path):
                result = env.callback(env, Node(path))
                self.assertEqual(result[0], 'compiled-object')
                flags = env.objects[-1][1]
                self.assertIn('-flto=1', flags['CCFLAGS'])
                self.assertNotIn('-fno-lto', sum((list(v) for v in flags.values()), []))
                self.assertIn('-fno-exceptions', flags['CXXFLAGS'])
        self.assertEqual(env['CCFLAGS'], ['-Os', '-fno-lto'])

    def test_framework_idf_assembly_and_unknown_sources_are_unchanged(self):
        env = Environment(); self.load('sticky_project_lto_pre.py', env)
        for path in ['/home/runner/.platformio/packages/framework-espidf/components/freertos/tasks.c', '/home/runner/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp', '/fixture/project/.pio/build/sticky/esp-idf/freertos/tasks.c', '/fixture/project/src/handler.S', '/elsewhere/unknown.cpp']:
            node = Node(path)
            self.assertIs(env.callback(env, node), node)
        self.assertFalse(env.objects)

    def test_mixed_optimizations_on_nonselected_sources_are_unchanged(self):
        env = Environment()
        env.update(CCFLAGS=['-Os', '-O2', '-flto=auto'],
                   CFLAGS=['-Og', '-DPROTECTED=1'],
                   CXXFLAGS=['-Ofast', '-fno-exceptions'],
                   ASFLAGS=['-Os', '-O2'])
        before = copy.deepcopy(dict(env))
        self.load('sticky_project_lto_pre.py', env)
        for path in ['/sdk/framework-espidf/tasks.c',
                     '/sdk/framework-arduinoespressif32/main.cpp',
                     '/fixture/project/src/start.S', '/fixture/project/lib/start.s',
                     '/fixture/project/freeink-sdk/libs/start.asm',
                     '/fixture/project/src-other/main.cpp',
                     '/fixture/project/lib-other/main.c',
                     '/fixture/project/.pio/libdeps-other/main.cc',
                     '/fixture/project/freeink-sdk/libs-other/main.cxx']:
            with self.subTest(path=path):
                node = Node(path)
                self.assertIs(env.callback(env, node), node)
                self.assertEqual(dict(env), before)
                self.assertFalse(env.objects)

    def test_mixed_optimizations_disabled_other_hardware_and_default_missing(self):
        for enabled, device, missing in [(False, 'sticky', False),
                                         (True, 'other-board', False),
                                         (True, 'sticky', True)]:
            with self.subTest(enabled=enabled, device=device, missing=missing):
                env = Environment(enabled, device)
                env.update(CCFLAGS=['-Os', '-O2'], CFLAGS=['-Og'],
                           CXXFLAGS=['-Ofast'], LINKFLAGS=['-O2', '-Og', '-fno-lto'])
                if missing:
                    env.GetProjectOption = lambda name, default=None: (
                        'sticky' if name == 'custom_firmware_device_type' else default)
                before = copy.deepcopy(dict(env))
                self.load('sticky_project_lto_pre.py', env)
                self.load('sticky_project_lto_post.py', env)
                self.assertIsNone(env.callback)
                self.assertEqual(dict(env), before)
                self.assertFalse(env.objects)

    def test_canonical_sdk_library_ownership_and_symlink_escape(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / 'project'
            library = project / 'freeink-sdk/libs/ui'
            library.mkdir(parents=True)
            (project / 'linked-ui').symlink_to(library, target_is_directory=True)
            outside = Path(temporary) / 'outside'
            outside.mkdir()
            (project / 'src').mkdir()
            (project / 'src/escape').symlink_to(outside, target_is_directory=True)
            env = Environment()
            env.subst = lambda name: {
                '$PROJECT_DIR': str(project), '$PROJECT_SRC_DIR': str(project / 'src'),
                '$PROJECT_LIB_DIR': str(project / 'lib'),
                '$PROJECT_LIBDEPS_DIR': str(project / '.pio/libdeps')}.get(name, name)
            self.load('sticky_project_lto_pre.py', env)
            env.callback(env, Node(str(project / 'linked-ui/helper.cpp')))
            self.assertEqual(env.objects[-1][1]['CCFLAGS'], ['-flto=1', '-Oz'])
            escape = Node(str(project / 'src/escape/helper.cpp'))
            self.assertIs(env.callback(env, escape), escape)
            self.assertEqual(len(env.objects), 1)

    def test_sticky_debug_remains_default_opt_out(self):
        config = configparser.ConfigParser(interpolation=None)
        config.read(ROOT / 'platformio.ini')
        self.assertEqual(config['env:sticky-debug']['extends'], 'firmware_tuned')
        for section in ('env:sticky-debug', 'firmware_tuned', 'base'):
            self.assertNotEqual(config[section].get('custom_sticky_project_lto', 'no'), 'yes')

    def test_final_link_has_only_oz_and_single_lto(self):
        env = Environment()
        env['LINKFLAGS'] = ['-Os', '-O2', '-Og', '-Ofast', '-O', '-O0', '-Oz',
                            '-flto', '-flto=auto', '-fno-lto', '-Wl,--gc-sections',
                            '-Wl,-Map,firmware.map', '-T', 'sections.ld']
        before = copy.deepcopy(dict(env))
        self.load('sticky_project_lto_post.py', env)
        self.assertEqual([f for f in env['LINKFLAGS']
                          if re.fullmatch(r'-O(?:[0-9]+|s|z|g|fast)?', f)], ['-Oz'])
        self.assertEqual([f for f in env['LINKFLAGS']
                          if f == '-flto' or f.startswith('-flto=')], ['-flto=1'])
        self.assertNotIn('-fno-lto', env['LINKFLAGS'])
        self.assertIn('-fuse-linker-plugin', env['LINKFLAGS'])
        self.assertEqual([f for f in env['LINKFLAGS']
                          if f not in ('-Oz', '-flto=1', '-fuse-linker-plugin')],
                         ['-Wl,--gc-sections', '-Wl,-Map,firmware.map', '-T', 'sections.ld'])
        for name in ('CCFLAGS', 'CFLAGS', 'CXXFLAGS'):
            self.assertEqual(env[name], before[name])

    def test_link_opt_in_does_not_change_framework_compile_flags(self):
        env = Environment(); self.load('sticky_project_lto_post.py', env)
        self.assertNotIn('-fno-lto', env['LINKFLAGS'])
        self.assertIn('-flto=1', env['LINKFLAGS'])
        self.assertIn('-fuse-linker-plugin', env['LINKFLAGS'])
        self.assertIn('-Wl,--gc-sections', env['LINKFLAGS'])
        self.assertEqual(env['CCFLAGS'], ['-Os', '-fno-lto'])

    def test_disabled_and_other_devices_are_untouched(self):
        for enabled, device in [(False, 'sticky'), (True, 'other-board')]:
            env = Environment(enabled, device)
            self.load('sticky_project_lto_pre.py', env)
            self.load('sticky_project_lto_post.py', env)
            self.assertIsNone(env.callback)
            self.assertEqual(env['LINKFLAGS'], ['-fno-lto', '-Wl,--gc-sections'])

    def test_profile_does_not_feed_lto_into_hybrid_sdk_configuration(self):
        config = configparser.ConfigParser(interpolation=None)
        config.read(ROOT/'platformio.ini')
        sticky = config['env:sticky']
        self.assertEqual(sticky.get('custom_sticky_project_lto'), 'yes')
        self.assertNotIn('-flto', sticky['build_flags'])
        self.assertNotIn('-fno-lto', sticky.get('build_unflags', ''))
        self.assertIn('pre:scripts/sticky_project_lto_pre.py', sticky['extra_scripts'])
        self.assertIn('post:scripts/sticky_project_lto_post.py', sticky['extra_scripts'])


if __name__ == '__main__':
    unittest.main()
