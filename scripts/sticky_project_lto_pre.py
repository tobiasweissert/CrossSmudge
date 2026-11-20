"""Use GCC -Oz with LTO for project C/C++, never the HybridCompile SDK.

Registered before frameworks run because PlatformIO 6.1.19 constructs project
objects before POST scripts. Object-local flags do not enter the SDK's project
options or mutate framework compilation environments. Assembly stays untouched.
"""
from pathlib import Path
import re


def enabled(env):
    return (env.GetProjectOption("custom_sticky_project_lto", "no") == "yes"
            and env.GetProjectOption("custom_firmware_device_type", "") == "sticky")


def without_profile_flags(flags):
    return [flag for flag in flags
            if not (isinstance(flag, str)
                    and (flag == "-fno-lto" or flag == "-flto" or flag.startswith("-flto=")
                         or re.fullmatch(r"-O(?:[0-9]+|s|z|g|fast)?", flag)))]


def register(env):
    if not enabled(env):
        return
    project = Path(env.subst("$PROJECT_DIR")).resolve()
    roots = [project / "freeink-sdk" / "libs"]
    for variable in ("$PROJECT_SRC_DIR", "$PROJECT_LIB_DIR", "$PROJECT_LIBDEPS_DIR"):
        value = env.subst(variable)
        if value and "$" not in value:
            roots.append(Path(value).resolve())
    roots = tuple(root.resolve() for root in roots)

    def project_object(object_env, node):
        source = Path(node.srcnode().get_abspath()).resolve()
        if source.suffix.lower() not in (".c", ".cc", ".cpp", ".cxx"):
            return node
        if not any(source.is_relative_to(root) for root in roots):
            return node
        flags = {name: without_profile_flags(object_env.get(name, []))
                 for name in ("CCFLAGS", "CFLAGS", "CXXFLAGS")}
        flags["CCFLAGS"].extend(["-flto=1", "-Oz"])
        return object_env.Object(node, **flags)

    env.AddBuildMiddleware(project_object)


Import("env")
register(env)
