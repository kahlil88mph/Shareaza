#!/usr/bin/env python3
"""Generate ShareazaOM.h and ShareazaOM_i.cpp from shareaza/Shareaza.idl.

On Windows, MIDL compiles the IDL. This is the small subset the Linux
build needs: the types (enums, structs), the interfaces as abstract C++
structs with MIDL's method names (get_X/put_X/putref_X for properties), the
cpp_quote lines, and the IID/CLSID/LIBID constants.

    idl2h.py Shareaza.idl out/ShareazaOM.h out/ShareazaOM_i.cpp
"""

import re
import sys

# IDL base types whose C++ spelling differs. IDL "long" is 32-bit, but long
# is 64-bit on Linux, so it becomes LONG.
TYPE_MAP = {"long": "LONG", "unsigned long": "ULONG", "hyper": "LONGLONG", "boolean": "BYTE"}


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def strip_attrs(s):
    # Remove [ ... ] attribute blocks (which may contain nested parentheses).
    out, depth = [], 0
    for c in s:
        if c == "[":
            depth += 1
            continue
        if c == "]" and depth:
            depth -= 1
            continue
        if not depth:
            out.append(c)
    return "".join(out)


def parse_uuid(attrs):
    m = re.search(r"uuid\(\s*([0-9A-Fa-f-]{36})\s*\)", attrs or "")
    return m.group(1) if m else None


def guid_init(u):
    p = u.split("-")
    d4 = p[3] + p[4]
    b = ", ".join("0x" + d4[i:i + 2] for i in range(0, 16, 2))
    return "{ 0x%s, 0x%s, 0x%s, { %s } }" % (p[0], p[1], p[2], b)


def map_type(t):
    t = " ".join(t.split())
    t = re.sub(r"\bSAFEARRAY\s*\(\s*\w+\s*\)", "SAFEARRAY*", t)	# as MIDL does
    for k, v in TYPE_MAP.items():
        t = re.sub(r"\b%s\b" % re.escape(k), v, t)
    return t


def parse_params(params):
    params = strip_attrs(params).strip()
    if not params or params == "void":
        return ""
    out = []
    for p in params.split(","):
        p = " ".join(p.split())
        m = re.match(r"^(.*?)([A-Za-z_][A-Za-z0-9_]*)$", p)
        if not m:
            raise SystemExit("cannot parse parameter: %r" % p)
        out.append("%s %s" % (map_type(m.group(1).strip()), m.group(2)))
    return ", ".join(out)


def tokens(src):
    """Yield (attrs, kind, text) for each top-level IDL item."""
    i, n = 0, len(src)
    attrs = None
    while i < n:
        while i < n and src[i].isspace():
            i += 1
        if i >= n:
            break
        if src[i] == "[":
            depth, j = 0, i
            while j < n:
                if src[j] == "[":
                    depth += 1
                elif src[j] == "]":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            attrs = src[i + 1:j]
            i = j + 1
            continue
        m = re.compile(r"cpp_quote\(\s*\"((?:[^\"\\]|\\.)*)\"\s*\)").match(src, i)
        if m:
            yield None, "quote", m.group(1).replace('\\"', '"')
            i = m.end()
            continue
        m = re.compile(r"(import|importlib)\b[^;]*;").match(src, i)
        if m:
            i = m.end()
            continue
        m = re.compile(r"library\s+(\w+)\s*\{").match(src, i)
        if m:
            yield attrs, "library", m.group(1)
            attrs = None
            i = m.end()
            continue
        m = re.compile(r"(interface|dispinterface)\s+(\w+)\s*;").match(src, i)
        if m:
            yield None, "forward", m.group(2)
            attrs = None
            i = m.end()
            continue
        m = re.compile(r"(interface|coclass)\s+(\w+)\s*(?::\s*(\w+))?\s*\{").match(src, i)
        if m:
            j = src.index("}", m.end())
            yield attrs, m.group(1), (m.group(2), m.group(3), src[m.end():j])
            attrs = None
            i = j + 1
            while i < n and src[i] in " \t\r\n;":
                i += 1
            continue
        m = re.compile(r"typedef\s+(enum|struct)\s*(\w+)?\s*\{").match(src, i)
        if m:
            j = src.index("}", m.end())
            k = src.index(";", j)
            name = src[j + 1:k].strip()
            yield attrs, "typedef", (m.group(1), name, src[m.end():j])
            attrs = None
            i = k + 1
            continue
        if src.startswith("};", i) or src[i] == "}":	# end of library
            i += 2 if src.startswith("};", i) else 1
            continue
        raise SystemExit("unexpected IDL at: %r" % src[i:i + 80])


def main():
    idl, out_h, out_c = sys.argv[1:4]
    src = strip_comments(open(idl, encoding="utf-8", errors="replace").read().replace("\r", ""))
    h = ["// Generated from shareaza/Shareaza.idl by linux/tools/idl2h.py. Do not edit.",
         "// Linux stand-in for the header MIDL generates on Windows.", "", "#pragma once", ""]
    c = ["// Generated from shareaza/Shareaza.idl by linux/tools/idl2h.py. Do not edit.",
         '#include <windows.h>', '#include <compat/com.h>', ""]
    ids = []
    for attrs, kind, val in tokens(src):
        if kind == "quote":
            h.append(val)
        elif kind == "library":
            u = parse_uuid(attrs)
            if u:
                ids.append(("LIBID_" + val, u))
        elif kind == "forward":
            h.append("struct %s;" % val)
        elif kind == "typedef":
            what, name, body = val
            body = strip_attrs(body)
            lines = [l.strip() for l in body.split("\n") if l.strip()]
            if what == "struct":
                lines = [map_type(l) for l in lines]
            h.append("typedef %s {" % what)
            h += ["\t" + l for l in lines]
            h.append("} %s;" % name)
            h.append("")
        elif kind == "coclass":
            name = val[0]
            u = parse_uuid(attrs)
            if u:
                ids.append(("CLSID_" + name, u))
        elif kind == "interface":
            name, base, body = val
            u = parse_uuid(attrs)
            if u:
                ids.append(("IID_" + name, u))
            h.append("struct %s : public %s" % (name, base or "IUnknown"))
            h.append("{")
            for decl in body.split(";"):
                decl = decl.strip()
                if not decl:
                    continue
                m = re.match(r"^(\[(?:[^\[\]]|\[[^\]]*\])*\])?\s*(\w+)\s+(\w+)\s*\((.*)\)\s*$", decl, re.S)
                if not m:
                    raise SystemExit("cannot parse method in %s: %r" % (name, decl))
                a, ret, meth, params = m.groups()
                a = a or ""
                prefix = "get_" if re.search(r"\bpropget\b", a) else "putref_" if re.search(r"\bpropputref\b", a) \
                    else "put_" if re.search(r"\bpropput\b", a) else ""
                h.append("\tvirtual %s STDMETHODCALLTYPE %s%s(%s) = 0;" % (map_type(ret), prefix, meth, parse_params(params)))
            h.append("};")
            h.append("")
    h.append("")
    for n, _ in ids:
        h.append("extern const %s %s;" % ("CLSID" if n.startswith("CLSID") else "IID", n))
    for n, u in ids:
        c.append("extern const %s %s = %s;" % ("CLSID" if n.startswith("CLSID") else "IID", n, guid_init(u)))
    open(out_h, "w").write("\n".join(h) + "\n")
    open(out_c, "w").write("\n".join(c) + "\n")


if __name__ == "__main__":
    main()
