#!/usr/bin/env python3
"""Wrap Windows-interface code in an engine source file in #ifdef _WIN32.

Used while porting, to keep the edits to upstream files small, uniform and
easy to review. Keeps the file's line endings.

    guard.py FILE include NAME.h [NAME.h ...]
        wrap these #include lines
    guard.py FILE function "SIGNATURE START" [--else "LINUX BODY"] [--note "COMMENT"]
        wrap the whole function whose definition starts with SIGNATURE START
        (up to its closing brace in column 0); with --else, give Linux a
        replacement definition
    guard.py FILE block "FIRST LINE" "LAST LINE" [--else TEXT] [--note COMMENT]
        wrap the lines from the one containing FIRST LINE to the next one
        containing LAST LINE, inclusive
"""

import sys


def load(path):
    raw = open(path, newline="", encoding="utf-8", errors="surrogateescape").read()
    nl = "\r\n" if "\r\n" in raw else "\n"
    return raw, nl


def save(path, text):
    open(path, "w", newline="", encoding="utf-8", errors="surrogateescape").write(text)


def opt(args, name):
    if name in args:
        i = args.index(name)
        v = args[i + 1]
        del args[i:i + 2]
        return v
    return None


def wrap(lines, a, b, nl, note, other):
    head = "#ifdef _WIN32" + ("\t// " + note if note else "")
    out = [head] + lines[a:b + 1]
    if other is not None:
        out += ["#else"] + other.replace("\\n", "\n").replace("\\t", "\t").split("\n")
    out += ["#endif // _WIN32"]
    return lines[:a] + out + lines[b + 1:]


def main():
    args = sys.argv[1:]
    note = opt(args, "--note")
    other = opt(args, "--else")
    path, mode = args[0], args[1]
    raw, nl = load(path)
    lines = raw.split(nl)
    if mode == "include":
        for name in args[2:]:
            idx = [i for i, l in enumerate(lines) if l.strip().startswith("#include") and ('"%s"' % name) in l]
            if len(idx) != 1:
                raise SystemExit("%s: #include \"%s\" found %d times" % (path, name, len(idx)))
            i = idx[0]
            if i > 0 and lines[i - 1].startswith("#ifdef _WIN32"):
                continue
            lines = wrap(lines, i, i, nl, None, None)
    elif mode == "function":
        sig = args[2]
        idx = [i for i, l in enumerate(lines) if l.startswith(sig)]
        if len(idx) != 1:
            raise SystemExit("%s: function %r found %d times" % (path, sig, len(idx)))
        a = idx[0]
        b = next(i for i in range(a + 1, len(lines)) if lines[i].startswith("}"))
        # include the comment banner directly above, if any
        while a > 0 and lines[a - 1].startswith("//"):
            a -= 1
        lines = wrap(lines, a, b, nl, note, other)
    elif mode == "block":
        first, last = args[2], args[3]
        a = next(i for i, l in enumerate(lines) if first in l)
        b = next(i for i in range(a, len(lines)) if last in lines[i])
        lines = wrap(lines, a, b, nl, note, other)
    else:
        raise SystemExit("unknown mode " + mode)
    save(path, nl.join(lines))


if __name__ == "__main__":
    main()
