"""Offline host checks. No credentials, sockets or installed ESP SDK required."""
from pathlib import Path
import subprocess as sp
import tempfile
import os
import re

here = Path(__file__).resolve().parent
root = here.parents[1]
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp)
    inc=Path(os.environ.get("ARDUINOJSON_INCLUDE_DIR",root/".pio/libdeps/sticky/ArduinoJson/src"))
    assert '"7.4.2"' in (inc/"ArduinoJson/version.hpp").read_text(), "Provide pinned ArduinoJson 7.4.2 via ARDUINOJSON_INCLUDE_DIR"
    for name in ("Arduino.h", "ESP_I2S.h", "WiFi.h", "HalClock.h", "AppNetworkPolicy.h", "esp_tls.h", "mbedtls/ssl.h", "esp_heap_caps.h", "esp_transport_ssl.h", "esp_crt_bundle.h", "mbedtls/build_info.h"):
        p = out / name
        p.parent.mkdir(exist_ok=True)
        p.write_text('#include "host.h"\n')
    for name in ("util/KeyboardEntryActivity", "util/OptionSelectionActivity", "util/ConfirmationActivity", "reader/QrDisplayActivity", "Activity", "network/WifiSelectionActivity"):
        p = out / "activities" / (name + ".h")
        p.parent.mkdir(exist_ok=True, parents=True)
        p.write_text('#include "ui.h"\n')
    # Compile the actual renderer language lookup and generated EN/DE tables,
    # not fixture labels. The release-wide generation is captured separately.
    import sys
    lang=out/"translations";lang.mkdir()
    for path in map(Path,sp.check_output(["git","ls-tree","-r","--name-only","HEAD","lib/I18n/translations"],cwd=root,text=True).splitlines()):
        (lang/path.name).write_bytes((root/path).read_bytes() if (root/path).exists() else sp.check_output(["git","show","HEAD:"+str(path)],cwd=root))
    sp.run([sys.executable,"-B",root/"scripts/gen_i18n.py",lang,out,"--strip-unused","--src-dirs",root/"src/activities/notes"],check=True,stdout=sp.DEVNULL)
    for name in ("I18n.cpp","I18n.h"):
        path=Path("lib/I18n")/name
        (out/name).write_bytes((root/path).read_bytes() if (root/path).exists() else sp.check_output(["git","show","HEAD:"+str(path)],cwd=root))
    (out / "fontIds.h").write_text('#include "ui.h"\n')
    def test(*args): sp.run([str(x) for x in args], check=True)
    # Compile real SD report admission, panic reason and ring entry boundaries.
    syscode=(root/"lib/hal/HalSystem.cpp").read_text()
    logging=(root/"lib/Logging/Logging.cpp").read_text()
    def function(source,name):
        pos=source.index(name);return source[pos:source.index("\n}",pos)+2]
    pickerPath="src/activities/network/WifiSelectionActivity.cpp"
    picker=(root/pickerPath).read_text() if (root/pickerPath).exists() else sp.check_output(["git","show","HEAD:"+pickerPath],cwd=root,text=True)
    (out/"picker.inc").write_text("\n".join(function(picker,"void WifiSelectionActivity::"+name+"(") for name in ("onComplete","onExit")))
    (out/"privacy.inc").write_text("\n".join([function(logging,"void addToLogRingBuffer("),function(syscode,"void IRAM_ATTR __wrap_panic_abort("),function(syscode,"void begin()"),function(syscode,"void checkPanic()")]))
    test("c++","-std=c++17","-DARDUINO_ARCH_ESP32","-DFREEINK_DEVICE_STICKY=1","-I",out,"-I",root/"lib/Logging",here/"privacy.cpp","-o",out/"privacy")
    test(out/"privacy")
    for name in ["protocol", "worker", "settings", "activity"]:
        flags = [] if name == "protocol" else ["-I",inc,"-I", out, "-I", here, "-DARDUINO_ARCH_ESP32", "-DFREEINK_DEVICE_STICKY=1", "-DMBEDTLS_HAVE_TIME_DATE", "-DCONFIG_MBEDTLS_CERTIFICATE_BUNDLE=1", "-DCONFIG_ESP_COREDUMP_ENABLE_TO_NONE=1", "-DCONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT=1", "-I", root / "lib/Logging", root / "src/activities/notes/DictationDevice.cpp"]
        if name in ("settings","activity"):flags.extend([out/"I18n.cpp",out/"I18nStrings.cpp"])
        if name == "activity": flags.append(root / "src/activities/notes/DictationActivity.cpp")
        test("c++", "-std=c++17", "-Wall", "-Werror", *flags, here / (name + ".cpp"), "-o", out / name)
        test(out / name)
        if name=="settings":
            test("c++","-std=c++17", *[f for f in flags if f!="-DCONFIG_ESP_COREDUMP_ENABLE_TO_NONE=1"],here/"blocked.cpp","-o",out/"blocked")
            test(out/"blocked")
    def payload(path):
        return (root/path).read_bytes() if (root/path).exists() else sp.check_output(["git","show","HEAD:"+str(path)],cwd=root)
    sources=sp.check_output(["git","ls-tree","-r","--name-only","HEAD","lib/Lua/src"],cwd=root,text=True).splitlines()
    for src in map(Path,sources):
        if src.suffix in (".c",".h"): (out/src.name).write_bytes(payload(src))
    package=out/"package";package.mkdir()
    for name in ("HalStorage.h","Logging.h"):
        (package/name).write_bytes(payload(Path("test/airdrop/browser_stubs")/name))
    (package/"Memory.h").write_bytes(payload(Path("lib/Memory/Memory.h")))
    test("c++","-std=c++20","-DSIMULATOR=1","-I",package,"-I",inc,"-I",root/"src/activities/apps/lua","-I",out,"-I",root/"src/activities/notes",here/"package.cpp","-o",out/"scan")
    test(out/"scan",root/"apps/cp-notes/manifest.json")
    objects=[]
    for src in sorted(out.glob("l*.c")):
        obj=src.with_suffix(".o");test("cc","-c",src,"-o",obj);objects.append(obj)
    test("c++", "-std=c++17", "-I", out, "-I", root / "src/activities/apps/lua", "-I",root/"src/activities/notes",here / "lua_host.cpp", *objects,"-lm", "-o", out / "lua")
    firmware=(root/"src/activities/notes/NotesFirmware.h").read_text()
    members=dict(re.findall(r'if \(!strcmp\(name,\s*"([^"]+)"\)\)\s+return R"NotesCode\((.*?)\)NotesCode";',firmware,re.S))
    assert set(members)=={"core.lua","storage.lua","graphics.lua","sticky_enhanced.lua"}
    for name,source in members.items():
        path=Path("apps/cp-notes")/("original_graphics.lua" if name=="graphics.lua" else name)
        assert source.encode()==payload(path), name
    sp.run([out / "lua", here / "notes.lua"], cwd=out, check=True)
