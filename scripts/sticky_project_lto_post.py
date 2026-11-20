"""Select GCC -Oz for the final LTO link after the stock framework is configured.

Only LINKFLAGS changes here. ESP-IDF/Arduino compilation and SDK configuration
stay non-LTO; assembly-referenced kernel globals therefore remain native roots.
"""
import re

Import("env")

if (env.GetProjectOption("custom_sticky_project_lto", "no") == "yes"
        and env.GetProjectOption("custom_firmware_device_type", "") == "sticky"):
    flags = [flag for flag in env.get("LINKFLAGS", [])
             if not (isinstance(flag, str)
                     and (flag == "-fno-lto" or flag == "-flto" or flag.startswith("-flto=")
                          or re.fullmatch(r"-O(?:[0-9]+|s|z|g|fast)?", flag)))]
    env.Replace(LINKFLAGS=flags + ["-flto=1", "-fuse-linker-plugin", "-Oz"])
