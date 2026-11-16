#!/usr/bin/env python3
"""Syntax-check every engine source file against the Linux compatibility layer.

Measures how much of the engine compiles on Linux, and what stops the rest.
"Engine" means shareaza/*.cpp minus the Windows interface (windows, dialogs,
controls, skins), which the Linux build replaces with Shareaza-Qt.

    linux/tools/syntax-check.py [build-dir] [--list] [--file X.cpp]

The build directory must have been configured with
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON (the flags are taken from it).
"""

import collections
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ENGINE = os.path.join(SRC, "shareaza")

# Windows interface: replaced by Shareaza-Qt, not ported.
GUI = re.compile(r"^(Ctrl|Wnd|Dlg|Page|Skin|Window|CoolMenu|CoolInterface|LiveList|RichDocument|"
                 r"RichElement|RichView|RichFragment|ImageFile|Flags|ShellIcons|Colors|Plugins|"
                 r"WizardSheet|FontManager|Graph|AutocompleteEdit|ComboListCtrl|Emoticons|"
                 r"ShareazaDataSource|DDEServer|IEProtocol|Scheduler|Splash|"
                 # the Windows application around the engine: setup wizard, plugin
                 # automation object, chat and menu windows, interface helpers
                 r"Wizard|Application\.|Shareaza\.cpp|ChatWindows|ComMenu|ComToolbar|FragmentBar|"
                 r"MetaList|ImageServices|WindowManager|BitziDownloader|FileExecutor|"
                 # Windows services the Linux build replaces: the registry (linux/engine/
                 # Registry.cpp), UPnP over COM (miniupnpc), Windows Firewall, AMSI
                 r"Registry\.cpp|UPnPFinder|UPnPNAT|Firewall|AntiVirus)")


HOST = os.path.join(SRC, "linux", "engine")


def engine_files():
    files = sorted(f for f in os.listdir(ENGINE) if f.endswith(".cpp") and not GUI.match(f))
    # the Linux host's own sources count too
    return files + sorted("../linux/engine/" + f for f in os.listdir(HOST) if f.endswith(".cpp"))


def flags(build):
    with open(os.path.join(build, "compile_commands.json")) as f:
        cc = json.load(f)
    entry = next(c for c in cc if c["file"].endswith("shareaza/Buffer.cpp"))
    args = shlex.split(entry["command"])
    out, skip = [], False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-c", "-MF", "-MT"):
            skip = True
            continue
        if a == "-MD":
            continue
        out.append(a)
    return args[0], out


def check(compiler, fl, name):
    p = subprocess.run([compiler, *fl, "-ferror-limit=0", "-fsyntax-only", os.path.join(ENGINE, name)],
                       capture_output=True, text=True, cwd=ENGINE)
    return name, p.returncode, p.stderr


def where(line):
    m = re.search(r"([A-Za-z0-9_./-]+?):\d+:\d+: (?:fatal )?error:", line)
    return os.path.basename(m.group(1)) if m else "?"


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    build = argv[0] if argv else os.path.join(SRC, "build-linux")
    compiler, fl = flags(build)
    files = engine_files()
    if "--file" in sys.argv:
        name = sys.argv[sys.argv.index("--file") + 1]
        print(check(compiler, fl, name)[2])
        return
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as ex:
        results = list(ex.map(lambda n: check(compiler, fl, n), files))

    clean = [n for n, rc, _ in results if rc == 0]
    broken = [(n, err) for n, rc, err in results if rc != 0]
    total = sum(e.count(" error: ") for _, e in broken)
    print(f"engine files: {len(files)}   compile: {len(clean)}   with errors: {len(broken)}   errors: {total}")

    first = collections.Counter()
    missing = collections.Counter()
    for n, err in broken:
        lines = [l for l in err.splitlines() if " error: " in l]
        first[where(lines[0]) if lines else "?"] += 1
        names = set(re.findall(r"(?:unknown type name|use of undeclared identifier|no member named) '([A-Za-z_0-9:]+)'", err))
        names |= set(re.findall(r"fatal error: '([^']+)' file not found", err))
        missing.update(names)
    print("\nfirst error is in (files stopped there):")
    for h, c in first.most_common(25):
        print(f"  {c:4d}  {h}")
    print("\nmissing names (number of files):")
    for h, c in missing.most_common(40):
        print(f"  {c:4d}  {h}")
    if "--list" in sys.argv:
        print("\ncompile:", " ".join(clean))
        print("\nerrors:", " ".join(f"{n}({where([l for l in e.splitlines() if ' error: ' in l][0])})" for n, e in broken))


if __name__ == "__main__":
    main()
