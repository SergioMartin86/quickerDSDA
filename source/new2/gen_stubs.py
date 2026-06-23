#!/usr/bin/env python3
"""
Generate no-op / zeroed stubs for symbols that the minimal headless core still
references but whose defining files were removed from the build.

For each undefined symbol we parse the REMOVED source files (which contain the
real definitions) and emit a matching stub: functions become no-ops returning a
zero value of the right type; data becomes a zero-initialized definition of the
right type. This keeps the link satisfied without pulling the subsystem in, and
-- because the simulation does not read display/sound output -- is bit-exact for
gameplay (verified per subsystem).

Usage:
  gen_stubs.py --undefined undef.txt --removed-list removed.txt \\
               --compile-commands build/compile_commands.json --out stubs.c
"""
import argparse
import json
import os
import shlex
import sys

import clang.cindex as cx

CK = cx.CursorKind
TK = cx.TypeKind


def find_resource_dir():
    import glob
    for pat in ("/usr/lib/llvm-18/lib/clang/18", "/usr/lib/clang/18*", "/usr/lib/llvm-*/lib/clang/*"):
        for d in sorted(glob.glob(pat), reverse=True):
            if os.path.exists(os.path.join(d, "include", "stddef.h")):
                return d
    return None


_RES = find_resource_dir()


def args_for(entry):
    raw = shlex.split(entry["command"])
    out, skip = [], False
    for i, a in enumerate(raw):
        if skip:
            skip = False
            continue
        if i == 0 or a in ("-c", "-Wfatal-errors") or a.startswith("-M"):
            continue
        if a == "-o":
            skip = True
            continue
        if a == entry["file"] or a == os.path.basename(entry["file"]):
            continue
        out.append(a)
    out += ["-D__STORAGE_MODIFIER=", "-ferror-limit=0", "-Wno-everything"]
    if _RES:
        out += ["-resource-dir", _RES]
    return out


def func_stub(c):
    rt = c.result_type.spelling
    params = []
    for i, a in enumerate(c.get_arguments()):
        ts = a.type.spelling
        if "[" in ts:
            # Array params carry the name before the '[': "unsigned char[16]" ->
            # "unsigned char a0[16]". Function-pointer params before the "(*)".
            j = ts.find("[")
            params.append(f"{ts[:j].strip()} a{i}{ts[j:]}")
        elif "(*)" in ts:
            params.append(ts.replace("(*)", f"(*a{i})", 1))
        else:
            params.append(f"{ts} a{i}")
    try:
        if c.type.kind == TK.FUNCTIONPROTO and c.type.is_function_variadic():
            params.append("...")
    except Exception:
        pass
    sig = ", ".join(params) if params else "void"
    if rt == "void":
        body = "{ }"
    else:
        # zero value of any type via a zero-initialized static
        body = f"{{ static {rt} _r; return _r; }}"
    return f"{rt} {c.spelling}({sig}) {body}"


def data_stub(c, deglob_syms):
    t = c.type
    name = c.spelling
    # Deglobalized data must keep __STORAGE_MODIFIER so the definition matches the
    # (thread-local) extern declaration in the new2 headers.
    mod = "__STORAGE_MODIFIER " if name in deglob_syms else ""
    if t.kind == TK.CONSTANTARRAY:
        # Format N-dimensional arrays by inserting the name before the first '['
        # of the full type spelling, e.g. "demostate_t[6][2]" -> "demostate_t x[6][2]".
        spell = t.spelling
        i = spell.find("[")
        base, dims = spell[:i].strip(), spell[i:]
        return f"{mod}{base} {name}{dims};"
    decl = t.spelling
    if "(*)" in decl:
        # Function/array pointer types carry the name inside the parens:
        # "unsigned long long (*)(void)" -> "unsigned long long (*name)(void)".
        return f"{mod}{decl.replace('(*)', f'(*{name})', 1)};"
    if t.is_const_qualified():
        return f"{mod}{decl} {name} = {{0}};"
    return f"{mod}{decl} {name};"


def scan_deglob_syms(root):
    """Names declared thread-local (__STORAGE_MODIFIER) in the deglobalized headers."""
    import re
    syms = set()
    pat = re.compile(r"__STORAGE_MODIFIER\b[^;{}=]*?([A-Za-z_]\w*)\s*(\[|;|=|,)")
    # Function/array-pointer globals carry the name inside (*name), which the
    # pattern above misses (name is followed by ')' not [;=,).
    fptr = re.compile(r"__STORAGE_MODIFIER\b[^;{}=]*?\(\s*\*\s*([A-Za-z_]\w*)\s*\)")
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if fn.endswith((".h", ".hpp")):
                try:
                    txt = open(os.path.join(dp, fn), errors="replace").read()
                except OSError:
                    continue
                for m in pat.finditer(txt):
                    syms.add(m.group(1))
                for m in fptr.finditer(txt):
                    syms.add(m.group(1))
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--undefined", required=True)
    ap.add_argument("--removed-list", required=True,
                    help="file with removed .c paths (relative to core root)")
    ap.add_argument("--root", required=True)
    ap.add_argument("--compile-commands", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--include", action="append", default=[],
                    help="header(s) to #include at the top of the stub file")
    args = ap.parse_args()

    undef = set()
    for line in open(args.undefined):
        s = line.strip()
        if s and not s.startswith(("collect2", "/")):
            undef.add(s)

    removed = set()
    for line in open(args.removed_list):
        s = line.strip()
        if s:
            removed.add(os.path.realpath(os.path.join(args.root, s)))

    cc_entries = json.load(open(args.compile_commands))

    def find_entry(rel):
        # The removed files are no longer compiled in new2, but the base core
        # compiles identical copies -- match by the prboom2/src/<rel> suffix.
        suffix = "prboom2/src/" + rel
        for e in cc_entries:
            if os.path.realpath(os.path.join(e["directory"], e["file"])).endswith(suffix):
                return e
        return None

    index = cx.Index.create()
    func_decls, data_decls = {}, {}
    for rel in sorted(set(open(args.removed_list).read().split())):
        entry = find_entry(rel)
        if entry is None:
            print(f"[stubs] no compile entry for {rel}, skipping", file=sys.stderr)
            continue
        path = os.path.realpath(os.path.join(entry["directory"], entry["file"]))
        cwd = os.getcwd()
        os.chdir(entry["directory"])
        try:
            tu = index.parse(path, args=args_for(entry))
        finally:
            os.chdir(cwd)
        for n in tu.cursor.walk_preorder():
            if n.location.file is None or os.path.realpath(n.location.file.name) != path:
                continue
            if n.kind == CK.FUNCTION_DECL and n.is_definition():
                func_decls.setdefault(n.spelling, n)
            elif n.kind == CK.VAR_DECL and n.is_definition() \
                    and n.semantic_parent.kind == CK.TRANSLATION_UNIT \
                    and n.storage_class != cx.StorageClass.STATIC:
                data_decls.setdefault(n.spelling, n)

    deglob_syms = scan_deglob_syms(args.root)
    out = ["/* Auto-generated no-op/zeroed stubs for removed subsystems. */",
           "/* Generated by source/new2/gen_stubs.py -- do not edit by hand. */",
           "#ifdef HAVE_CONFIG_H", "#include \"config.h\"", "#endif"]
    out += [f'#include "{h}"' for h in args.include]
    out.append("")
    done, missing = set(), []
    fn = dt = 0
    for s in sorted(undef):
        if s in done:
            continue
        if s in func_decls:
            out.append(func_stub(func_decls[s]))
            fn += 1
        elif s in data_decls:
            out.append(data_stub(data_decls[s], deglob_syms))
            dt += 1
        else:
            missing.append(s)
            continue
        done.add(s)

    open(args.out, "w").write("\n".join(out) + "\n")
    print(f"[stubs] wrote {fn} function + {dt} data stubs to {args.out}")
    if missing:
        print(f"[stubs] {len(missing)} undefined symbols NOT found in removed files "
              f"(defined elsewhere / generated): {', '.join(missing[:30])}"
              f"{' ...' if len(missing) > 30 else ''}")


if __name__ == "__main__":
    main()
