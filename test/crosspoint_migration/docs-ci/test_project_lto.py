#!/usr/bin/env python3
"""Project-only LTO policy checks; no target compiler or SDK changes."""
from pathlib import Path
import configparser
import runpy
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
