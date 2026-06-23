#!/usr/bin/env python3
"""
Clang-AST deglobalizer for quickerDSDA's new2 core.

Replaces the fragile line-regex `deglobalizer.py`. Given a compile_commands.json
(so every translation unit parses with its exact flags) it finds every variable
with *static storage duration* that is not top-level-const, and prefixes its
declaration with the `__STORAGE_MODIFIER` token (which the build defines to
`__thread`). This makes each worker thread own a private copy of all mutable
DSDA-Doom global state.

Why AST and not regex:
  * Catches function-`static` locals (mutable state across calls) -- the regex
    only matched file-scope names and silently missed these.
  * Distinguishes stack autos (skip) from static-storage globals (annotate) via
    storage class + linkage, not text.
  * Handles pointers, arrays, multi-declarators (`int a, b, c;`), struct-typed
    globals, and `extern` declarations in headers correctly.
  * Skips top-level-const (rodata) -- matching what `nm` reports as writable.

GCC ordering rule: `__thread` must come immediately *after* a `static`/`extern`
storage-class keyword (`static __thread int x;`), so the token is inserted after
that keyword when present, else at the declaration start.

Cross-validation: with --nm-symbols <file> (output of `nm` filtered to B/b/D/d),
asserts every writable global symbol got an annotation, reporting any miss.

Usage:
  deglobalize.py --compile-commands build/compile_commands.json \\
                 --root source/new2/core [--apply] [--nm-symbols globals.txt]
Without --apply it only reports (dry run).
"""

import argparse
import glob
import json
import os
import shlex
import sys

import clang.cindex as cx


def find_resource_dir():
    """Locate clang's resource directory (its built-in headers: stddef.h, ...).

    The pip `libclang` wheel ships libclang.so but NOT these headers, so without
    pointing at a resource dir every TU parses only partially (size_t et al.
    undefined), silently degrading the AST. Prefer a system clang 18 install.
    """
    for pat in ("/usr/lib/llvm-18/lib/clang/18", "/usr/lib/clang/18*",
                "/usr/lib/llvm-*/lib/clang/*", "/usr/lib/clang/*"):
        for d in sorted(glob.glob(pat), reverse=True):
            if os.path.exists(os.path.join(d, "include", "stddef.h")):
                return d
    return None


_RESOURCE_DIR = find_resource_dir()

# Storage-class / linkage enums (referenced often)
SC = cx.StorageClass
LK = cx.LinkageKind
CK = cx.CursorKind
TK = cx.TypeKind

# Flags we must drop from the recorded compile command before handing to libclang
_DROP_EXACT = {"-c", "-o", "-Wfatal-errors", "-Winvalid-pch", "-pipe",
               "-fdiagnostics-color=always", "--coverage", "-pedantic-errors"}
_DROP_PREFIX = ("-M",)  # dependency-generation flags


def parse_args_from_entry(entry):
    """Return (lang_args, source_path, directory) for a compile_commands entry."""
    directory = entry["directory"]
    if "arguments" in entry:
        raw = list(entry["arguments"])
    else:
        raw = shlex.split(entry["command"])
    source = entry["file"]

    args = []
    skip_next = False
    for i, a in enumerate(raw):
        if skip_next:
            skip_next = False
            continue
        if i == 0:
            continue  # compiler driver (cc / c++)
        if a == "-o":
            skip_next = True
            continue
        if a in _DROP_EXACT or any(a.startswith(p) for p in _DROP_PREFIX):
            continue
        if a == source or a == os.path.basename(source):
            continue
        args.append(a)
    # Parse the un-deglobalized copy: make the token expand to nothing, and never
    # bail early so we always get a usable AST.
    args += ["-D__STORAGE_MODIFIER=", "-ferror-limit=0", "-Wno-everything"]
    # Clang's own headers (stddef.h, ...) -- without this the AST parses only
    # partially and initializer references (address-takes) are silently missed.
    if _RESOURCE_DIR:
        args += ["-resource-dir", _RESOURCE_DIR]
    return args, source, directory


def needs_tls(node):
    """True iff this VarDecl has static storage duration and is not top-level const."""
    if node.kind != CK.VAR_DECL:
        return False
    t = node.type
    if t.is_const_qualified():
        return False  # rodata -- matches nm's R/r classification
    sc = node.storage_class
    if sc in (SC.STATIC, SC.EXTERN):
        return True            # file-static, function-static, or extern decl
    if sc == SC.NONE:
        # File-scope global has linkage; a stack auto has NO_LINKAGE -> skip it.
        return node.linkage != LK.NO_LINKAGE
    return False               # register / auto / invalid


def is_cpp(path):
    return path.endswith((".cpp", ".cc", ".cxx", ".C"))


def static_duration(node):
    sc = node.storage_class
    return sc in (SC.STATIC, SC.EXTERN) or (sc == SC.NONE and node.linkage != LK.NO_LINKAGE)


# ---- Hot-path call-graph analysis (which globals MUST be thread-local) --------
# A global must be TLS iff it is WRITTEN by a function reachable from the per-tic
# entry points (tick / save-load / level-load). Doom's action functions run via
# the state machine's function pointers, so any address-taken A_*/T_*/*Thinker is
# also a reachability root. Globals only written during one-time boot (reachable
# only from headlessMain) are deliberately excluded -- they are benign to share.
_HOTPATH_ENTRY = {
    "headlessRunSingleTick", "G_Ticker", "P_Ticker", "G_DoLoadLevel",
    "P_SetupLevel", "dsda_UnArchiveAll", "dsda_ArchiveAll", "headlessSetTickCommand",
}
_ASSIGN_OPS = {"=", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>="}


def _written_global(node):
    """If this assignment/inc-dec writes a static-duration global, return its name."""
    kids = list(node.get_children())
    if not kids:
        return None
    for c in kids[0].walk_preorder():
        if c.kind == CK.DECL_REF_EXPR:
            r = c.referenced
            if r is not None and r.kind == CK.VAR_DECL and static_duration(r):
                return r.spelling
            return None
    return None


def analyze_function(fn, writes, calls):
    name = fn.spelling
    w = writes.setdefault(name, set())
    cl = calls.setdefault(name, set())
    for n in fn.walk_preorder():
        if n.kind in (CK.BINARY_OPERATOR, CK.COMPOUND_ASSIGNMENT_OPERATOR):
            toks = [t.spelling for t in n.get_tokens()]
            if any(t in _ASSIGN_OPS for t in toks) and "==" not in toks:
                g = _written_global(n)
                if g:
                    w.add(g)
        elif n.kind == CK.UNARY_OPERATOR:
            toks = [t.spelling for t in n.get_tokens()]
            if "++" in toks or "--" in toks:
                g = _written_global(n)
                if g:
                    w.add(g)
        elif n.kind == CK.CALL_EXPR:
            r = n.referenced
            if r is not None:
                cl.add(r.spelling)


def compute_must_tls(writes, calls, addr_funcs):
    roots = set(_HOTPATH_ENTRY)
    roots |= {f for f in addr_funcs
              if f.startswith(("A_", "T_")) or f.endswith("Thinker")}
    seen, stack = set(), list(roots)
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        seen.add(f)
        stack.extend(c for c in calls.get(f, ()) if c not in seen)
    must = set()
    for f in seen:
        must |= writes.get(f, set())
    return must


_FUNC_KINDS = (CK.FUNCTION_DECL, CK.CXX_METHOD, CK.CONSTRUCTOR, CK.DESTRUCTOR,
               CK.FUNCTION_TEMPLATE, CK.LAMBDA_EXPR)


def collect_address_taken(cursor, out, in_function=False):
    """Accumulate names of static-duration globals whose address is referenced in a
    FILE-SCOPE initializer (outside any function). Such initializers must be
    constant, so a reference is a (link-constant) address-take, e.g.
    `&doom_S_sfx[x]`. These referents are read-only data tables and must stay
    shared, else the initializer becomes non-constant. (Address-takes inside
    `static` LOCALS are handled separately by de-static-ing the local, so the
    referenced gameplay globals can still be made thread-local.)
    """
    for c in cursor.get_children():
        if c.kind in _FUNC_KINDS:
            collect_address_taken(c, out, in_function=True)
        else:
            if not in_function and c.kind == CK.DECL_REF_EXPR:
                ref = c.referenced
                if ref is not None and ref.kind == CK.VAR_DECL and static_duration(ref):
                    out.add(ref.spelling)
            collect_address_taken(c, out, in_function)


def init_references_global(vardecl):
    """True if this VarDecl's initializer takes the address of a static-duration
    global (so a `static` local with such an init must be de-static-ed to let that
    global become thread-local)."""
    for c in vardecl.walk_preorder():
        if c.kind == CK.DECL_REF_EXPR:
            ref = c.referenced
            if ref is not None and ref.kind == CK.VAR_DECL and static_duration(ref) \
               and ref.spelling != vardecl.spelling:
                return True
    return False


def static_token_span(node):
    """(offset, length) of the leading `static` keyword token of this decl, or None."""
    for t in node.get_tokens():
        if t.spelling == "static":
            return (t.extent.start.offset, len("static"))
        # only the very first token may be the storage class
        return None
    return None


def cpp_tls_unsafe(node):
    """`__thread` is illegal on non-trivially-copyable C++ types; flag those.

    C structs/unions are always trivially copyable, so this only matters in C++
    TUs. We conservatively flag record-typed globals in C++ for manual handling
    (e.g. the ambient.cpp std::unordered_map -> thread_local in headless.patch).
    """
    canon = node.type.get_canonical()
    return canon.kind == TK.RECORD


def is_continuation_declarator(node, file_cache):
    """True if this VarDecl is the 2nd+ declarator in a comma list (`int a, b;`).

    The storage-class specifier (and thus __STORAGE_MODIFIER/__thread) on the first
    declarator already applies to the whole declaration, so the continuation must
    NOT be annotated -- `int a, __thread b;` is invalid. Detected by the nearest
    non-whitespace source char before the declarator being a comma.
    """
    f = node.location.file
    if f is None:
        return False
    path = os.path.realpath(f.name)
    data = file_cache.get(path)
    if data is None:
        data = open(path, "rb").read()
        file_cache[path] = data
    i = node.extent.start.offset - 1
    while i >= 0 and data[i:i + 1].isspace():
        i -= 1
    return i >= 0 and data[i:i + 1] == b","


def file_bytes(fpath, cache):
    data = cache.get(fpath)
    if data is None:
        data = open(fpath, "rb").read()
        cache[fpath] = data
    return data


def init_expr_of(node):
    """The initializer expression child of a VarDecl, or None."""
    last = None
    for c in node.get_children():
        if c.kind not in (CK.TYPE_REF, CK.TEMPLATE_REF, CK.NAMESPACE_REF):
            last = c
    return last


def capture_construct(node, cache):
    """Plan to convert a file-scope initialized var to a runtime-initialized one.

    Returns a dict with the declaration edits (drop initializer, size an implicit
    array) and the per-thread assignment statements that reproduce the original
    initializer at runtime -- legal even when it takes the address of a TLS global,
    because runtime/compound-literal initialization is not a constant-expr context.
    Returns None if the construct cannot be safely transformed.
    """
    init = init_expr_of(node)
    if init is None or not init.location.file:
        return None
    fpath = os.path.realpath(node.location.file.name)
    data = file_bytes(fpath, cache)
    init_start, init_end = init.extent.start.offset, init.extent.end.offset
    # Find the '=' just before the initializer.
    eq = data.rfind(b"=", node.extent.start.offset, init_start)
    if eq < 0:
        return None
    decl_edits = [(eq, init_end - eq, "")]  # delete "= <init>"
    stmts = []
    is_array = node.type.kind == TK.CONSTANTARRAY
    if is_array:
        n = node.type.element_count
        elem_type = node.type.get_array_element_type().spelling
        elems = [c for c in init.get_children()]
        if len(elems) != n:
            return None  # designated/partial init -- don't risk it
        for i, e in enumerate(elems):
            src = data[e.extent.start.offset:e.extent.end.offset].decode("utf-8", "replace")
            stmts.append(f"  {node.spelling}[{i}] = ({elem_type}){src};")
        # Size an implicit `[]` so the array is complete without its initializer.
        toks = list(node.get_tokens())
        for a, b in zip(toks, toks[1:]):
            if a.spelling == "[" and b.spelling == "]":
                decl_edits.append((b.extent.start.offset, 0, str(n)))
                break
    else:
        src = data[init_start:init_end].decode("utf-8", "replace")
        stmts.append(f"  {node.spelling} = {src};")
    return {"fpath": fpath, "decl_edits": decl_edits, "stmts": stmts}


def is_macro_synth(node):
    """True if this VarDecl is synthesized by a macro expansion (its own name is
    not among its declaration tokens). Such decls cannot be annotated in source."""
    toks = list(node.get_tokens())
    return not toks or (bool(node.spelling)
                        and node.spelling not in {t.spelling for t in toks})


def insert_point(node):
    """Edit to annotate this decl, as (offset, remove_len, text); None to skip."""
    toks = list(node.get_tokens())
    if not toks:
        return None
    head = toks[0].spelling
    if head in ("__STORAGE_MODIFIER", "__thread", "thread_local", "_Thread_local"):
        return None  # already deglobalized (idempotent re-runs)
    if head in ("static", "extern"):
        # Insert AFTER the storage-class keyword: "static __STORAGE_MODIFIER ...".
        return (toks[0].extent.end.offset, 0, " __STORAGE_MODIFIER")
    return (node.extent.start.offset, 0, "__STORAGE_MODIFIER ")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compile-commands", required=True)
    ap.add_argument("--root", required=True,
                    help="only annotate declarations in files under this dir")
    ap.add_argument("--apply", action="store_true",
                    help="write edits to disk (default: dry-run report)")
    ap.add_argument("--nm-symbols", default=None,
                    help="file of writable symbol names (nm B/b/D/d) to cross-check")
    ap.add_argument("--exclude-symbols", default=None,
                    help="file of symbol names to NEVER annotate (one per line, '#' "
                         "comments) -- e.g. immutable LUTs whose address is taken in a "
                         "static initializer (finesine/finetangent)")
    ap.add_argument("--libclang", default=None,
                    help="explicit path to libclang.so (else use bundled)")
    args = ap.parse_args()

    if args.libclang:
        cx.Config.set_library_file(args.libclang)

    root = os.path.realpath(args.root)
    cc = json.load(open(args.compile_commands))
    cc = [e for e in cc if os.path.realpath(os.path.join(e["directory"], e["file"])).startswith(root)]
    # Tool handles C TUs only. C++ TUs define very few globals (MAPINFO/UMAPINFO/
    # UDMF/ambient parsers) which are non-trivial types needing `thread_local`
    # (not `__thread`); those are handled explicitly in headless.patch. Excluding
    # .cpp here also avoids accidentally putting __thread on a std::unordered_map.
    cc = [e for e in cc if not is_cpp(e["file"])]
    print(f"[deglob] {len(cc)} C translation units under {root}")

    exclude_syms = set()
    if args.exclude_symbols:
        for line in open(args.exclude_symbols):
            line = line.split("#", 1)[0].strip()
            if line:
                exclude_syms.add(line)
        print(f"[deglob] excluding {len(exclude_syms)} symbols by name (left shared)")

    index = cx.Index.create()
    # Candidate edits: (fpath, offset, text, symbol, is_extern). Filtered after all
    # TUs are seen, so we can drop extern-only decls of symbols never defined in a
    # processed C TU (i.e. defined in .cpp) -- avoids TLS-attribute mismatches.
    candidates = []
    defined_syms = set()        # symbols with a non-extern definition under root (C)
    address_taken_syms = set()  # globals whose address is used in a file-scope init
    annotated_syms = set()
    const_skipped_syms = set()  # static-duration vars left shared because top-level const
    seen_root_syms = set()      # every VAR_DECL spelling defined under root (any storage)
    macro_skipped = set()       # benign block-local statics synthesized by macros
    macro_file_skipped = set()  # FILE-SCOPE globals emitted by a macro -- need a macro-def edit
    decl_only_skipped = set()   # anon-struct pointer static-locals (no safe insert point)
    destatic = {}               # fpath -> {(offset, len, "")} : `static` keywords to delete
    destatic_syms = set()
    cpp_skipped = []  # (file, line, name) skipped as non-trivial C++ records
    file_cache = {}             # path -> bytes, for continuation-declarator detection
    n_tu_err = 0
    # Hot-path call-graph data + file-scope construct plans (for runtime-init).
    writes, calls, addr_funcs = {}, {}, set()
    constructs = {}             # name -> {plan, refs} for small initialized file-scope vars
    uncapturable_refs = set()   # globals referenced by too-big/uncapturable constructs
    CONSTRUCT_MAX_ELEMS = 64

    for i, entry in enumerate(cc):
        cargs, source, directory = parse_args_from_entry(entry)
        abspath = os.path.realpath(os.path.join(directory, source))
        cwd = os.getcwd()
        try:
            os.chdir(directory)
            tu = index.parse(abspath, args=cargs,
                             options=cx.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        except Exception as e:
            print(f"[deglob] PARSE-FAIL {source}: {e}", file=sys.stderr)
            n_tu_err += 1
            os.chdir(cwd)
            continue
        finally:
            os.chdir(cwd)

        # Globals whose address is taken in any file-scope initializer in this TU
        # must stay shared (else the initializer becomes non-constant).
        collect_address_taken(tu.cursor, address_taken_syms)

        # Hot-path call graph: analyze each function definition for writes/calls.
        for fn in tu.cursor.get_children():
            if fn.kind == CK.FUNCTION_DECL and fn.is_definition():
                analyze_function(fn, writes, calls)

        for node in tu.cursor.walk_preorder():
            # Functions whose address is taken anywhere (e.g. action pointers in
            # states[]) are call-graph roots reachable at runtime.
            if node.kind == CK.DECL_REF_EXPR:
                r = node.referenced
                if r is not None and r.kind == CK.FUNCTION_DECL:
                    addr_funcs.add(r.spelling)
            # File-scope initialized var referencing a DIFFERENT global -> a
            # candidate construct for runtime-init (if its referent must be TLS).
            if node.kind == CK.VAR_DECL and node.semantic_parent is not None \
               and node.semantic_parent.kind == CK.TRANSLATION_UNIT \
               and static_duration(node) and node.location.file is not None \
               and os.path.realpath(node.location.file.name).startswith(root) \
               and node.spelling not in constructs:
                refs = set()
                init = init_expr_of(node)
                if init is not None:
                    for c in init.walk_preorder():
                        if c.kind == CK.DECL_REF_EXPR:
                            r = c.referenced
                            if r is not None and r.kind == CK.VAR_DECL \
                               and static_duration(r) and r.spelling != node.spelling:
                                refs.add(r.spelling)
                if refs:
                    too_big = node.type.kind == TK.CONSTANTARRAY \
                        and node.type.element_count > CONSTRUCT_MAX_ELEMS
                    plan = None if too_big else capture_construct(node, file_cache)
                    if plan is not None:
                        constructs[node.spelling] = {"plan": plan, "refs": refs}
                    else:
                        uncapturable_refs |= refs

        for node in tu.cursor.walk_preorder():
            if node.kind == CK.VAR_DECL and node.location.file is not None \
               and os.path.realpath(node.location.file.name).startswith(root):
                # Record static-duration const vars we deliberately leave shared,
                # so the nm cross-check can tell "correctly immutable" from "missed".
                sc = node.storage_class
                static_dur = sc in (SC.STATIC, SC.EXTERN) or \
                    (sc == SC.NONE and node.linkage != LK.NO_LINKAGE)
                seen_root_syms.add(node.spelling)
                if static_dur and node.type.is_const_qualified():
                    const_skipped_syms.add(node.spelling)
                # A non-extern static-duration var is a definition we will process.
                if static_dur and sc != SC.EXTERN and not node.type.is_const_qualified() \
                   and node.spelling not in exclude_syms \
                   and not is_cpp(os.path.realpath(node.location.file.name)):
                    defined_syms.add(node.spelling)
            if not needs_tls(node):
                continue
            if node.spelling in exclude_syms:
                continue
            loc = node.location
            if loc.file is None:
                continue
            fpath = os.path.realpath(loc.file.name)
            if not fpath.startswith(root):
                continue
            # A C header may be included by a .c TU but declare a C++-only global;
            # only annotate decls living in .c/.h files (never .cpp).
            if is_cpp(fpath):
                continue
            # A `static` LOCAL whose initializer takes the address of a global
            # (e.g. spechit_overrun_param = { &spechit, &numspechit, ... }): make
            # it an auto/scratch local by deleting `static`, so those per-tick
            # gameplay globals can still be made thread-local. (Matches the hand
            # core, which dropped `static` from these scratch structs.)
            if node.storage_class == SC.STATIC and node.semantic_parent is not None \
               and node.semantic_parent.kind != CK.TRANSLATION_UNIT \
               and init_references_global(node):
                span = static_token_span(node)
                if span is not None:
                    destatic.setdefault(fpath, set()).add((span[0], span[1], ""))
                    destatic_syms.add(node.spelling)
                    continue
            if is_macro_synth(node):
                # Distinguish a file-scope global generated by a code-emitting
                # macro (e.g. IMPLEMENT_BLOCK_MEMORY_ALLOC_ZONE -> a real global
                # that MUST be made TLS at the macro definition) from a benign
                # block-local once-guard inside a macro body.
                if node.semantic_parent is not None \
                   and node.semantic_parent.kind == CK.TRANSLATION_UNIT \
                   and node.spelling not in address_taken_syms:
                    macro_file_skipped.add(node.spelling)
                else:
                    macro_skipped.add(node.spelling)
                continue
            # 2nd+ declarator in `int a, b;` -- covered by the first declarator's
            # storage class; annotating it would be mid-declaration and invalid.
            if is_continuation_declarator(node, file_cache):
                annotated_syms.add(node.spelling)  # effectively TLS via the list
                continue
            # Anonymous-struct-typed pointer static-local, e.g.
            # `static struct {..} *hash;` -- libclang's extent starts at the
            # declarator name (after the `*`/struct body), so there is no safe
            # token to insert before. These are rare lazily-built caches; leave
            # them shared (matches the hand-built core).
            toks0 = list(node.get_tokens())
            if toks0 and toks0[0].spelling == node.spelling:
                decl_only_skipped.add(node.spelling)
                continue
            ip = insert_point(node)
            if ip is None:
                continue
            offset, rlen, text = ip
            candidates.append((fpath, offset, rlen, text, node.spelling,
                               node.storage_class == SC.EXTERN))

        if (i + 1) % 50 == 0:
            print(f"[deglob] parsed {i+1}/{len(cc)} TUs ...")

    # Keep a definition unconditionally; keep an extern decl only if the symbol is
    # actually defined in a processed C TU. This drops extern decls of globals that
    # live in .cpp files (udmf/Maps/doom_mapinfo/...), which would otherwise create
    # a thread-local-vs-plain mismatch against their unannotated .cpp definitions.
    # Globals whose address is taken in a file-scope initializer are forced shared
    # (after transitive closure across all TUs), independent of where declared.
    print(f"[deglob] address-taken-in-static-init globals (forced shared): {len(address_taken_syms)}")

    # --- Hot-path TLS: rescue gameplay globals the address-taken rule over-shared ---
    # A global written on the per-tic / save-load / level-load path MUST be TLS.
    # If the address-taken rule shared it (referenced in a file-scope init), force
    # it TLS -- but only if every file-scope construct taking its address is small
    # enough to runtime-initialize (else, e.g. the 500-entry dsda_config, leave it
    # shared: those referents are read-mostly config, harmless to share).
    must_tls = compute_must_tls(writes, calls, addr_funcs)
    force_tls = (must_tls & address_taken_syms) - uncapturable_refs
    rt_constructs = {n: i for n, i in constructs.items() if i["refs"] & force_tls}
    address_taken_syms -= force_tls   # let these be annotated TLS
    print(f"[deglob] hot-path must-TLS: {len(must_tls)}  force-TLS (rescued from shared): "
          f"{len(force_tls)}  runtime-init constructs: {len(rt_constructs)}")
    if force_tls:
        print(f"[deglob]   force-TLS: {', '.join(sorted(force_tls))}")

    edits = {fp: set(s) for fp, s in destatic.items()}  # seed with `static` deletions
    dropped_extern = set()
    dropped_addr = set()
    for fpath, offset, rlen, text, sym, is_extern in candidates:
        if sym in address_taken_syms:
            dropped_addr.add(sym)
            continue
        if is_extern and sym not in defined_syms:
            dropped_extern.add(sym)
            continue
        edits.setdefault(fpath, set()).add((offset, rlen, text))
        annotated_syms.add(sym)

    # Runtime-init the rescued constructs: drop their static initializers and emit
    # a per-file init function (appended to the file so it sees the locals/types),
    # plus a master __deglob_init_tls() (appended to d_main.c) that calls them all.
    # Called once per thread from emuInstanceBase::initialize() after headlessMain.
    per_file_stmts = {}
    for name, info in sorted(rt_constructs.items()):
        plan = info["plan"]
        fp = plan["fpath"]
        for e in plan["decl_edits"]:
            edits.setdefault(fp, set()).add(e)
        per_file_stmts.setdefault(fp, []).extend([f"  // {name}"] + plan["stmts"])
    init_func_names = []
    for fp, stmts in sorted(per_file_stmts.items()):
        fid = "".join(ch if ch.isalnum() else "_" for ch in os.path.basename(fp))
        fname = f"__deglob_tls_init_{fid}"
        init_func_names.append(fname)
        body = "\n".join(stmts)
        func = f"\n/* deglobalizer: per-thread TLS pointer init */\nvoid {fname}(void)\n{{\n{body}\n}}\n"
        edits.setdefault(fp, set()).add((len(file_bytes(fp, file_cache)), 0, func))
    if init_func_names:
        dmain = os.path.realpath(os.path.join(root, "prboom2/src/d_main.c"))
        decls = "".join(f"  extern void {n}(void);\n" for n in init_func_names)
        calls_ = "".join(f"  {n}();\n" for n in init_func_names)
        master = (f"\n/* deglobalizer: master per-thread TLS pointer initializer; called\n"
                  f"   once per thread from the headless host after headlessMain. */\n"
                  f"void __deglob_init_tls(void)\n{{\n{decls}{calls_}}}\n")
        edits.setdefault(dmain, set()).add((len(file_bytes(dmain, file_cache)), 0, master))

    total_edits = sum(len(s) for s in edits.values())
    print(f"[deglob] files to edit: {len(edits)}  edits: {total_edits}  "
          f"distinct symbols: {len(annotated_syms)}  de-static'd locals: {len(destatic_syms)}  "
          f"TU parse failures: {n_tu_err}")
    if macro_file_skipped:
        print(f"[deglob] *** WARNING: {len(macro_file_skipped)} FILE-SCOPE globals are "
              f"emitted by a code-generating macro and were NOT annotated. These are "
              f"shared across threads unless the MACRO DEFINITION is annotated by hand "
              f"(see regenerate.sh macro-tls step): {', '.join(sorted(macro_file_skipped))}")
    if dropped_extern:
        print(f"[deglob] dropped {len(dropped_extern)} extern-only decls "
              f"(symbols defined in .cpp / excluded): "
              f"{', '.join(sorted(dropped_extern)[:12])}{' ...' if len(dropped_extern) > 12 else ''}")
    if cpp_skipped:
        print(f"[deglob] WARNING: {len(cpp_skipped)} non-trivial C++ record globals "
              f"skipped (need thread_local by hand):")
        for f, ln, nm in cpp_skipped:
            print(f"           {f}:{ln}  {nm}")

    # Cross-validation against the linker's writable-symbol set.
    if args.nm_symbols:
        import re
        import subprocess

        def demangle(name):
            if not name.startswith("_Z"):
                return name
            try:
                out = subprocess.run(["c++filt", name], capture_output=True, text=True)
                return out.stdout.strip() or name
            except Exception:
                return name

        def normalize(name):
            # GCC renames function-static locals to "name.N" -> strip the suffix.
            name = re.sub(r"\.[0-9]+$", "", name)
            # C++ symbols are mangled; demangle and take the trailing identifier.
            if name.startswith("_Z"):
                dm = demangle(name)
                dm = re.sub(r"\(.*$", "", dm)           # drop function args
                dm = dm.split("::")[-1].strip()          # trailing component
                dm = re.sub(r"\.[0-9]+$", "", dm)
                name = dm or name
            return name

        nm_syms = set()
        for line in open(args.nm_symbols):
            parts = line.split()
            if len(parts) >= 3 and parts[1] in ("B", "b", "D", "d"):
                nm_syms.add(parts[2])
            elif len(parts) == 2 and parts[0] in ("B", "b", "D", "d"):
                nm_syms.add(parts[1])

        # Restrict to symbols the tool actually saw declared under the core root;
        # this excludes third-party statics (jaffarCommon/SDL) that get compiled
        # into core objects via header inclusion but are not DSDA state.
        seen_norm = {normalize(s) for s in seen_root_syms} | seen_root_syms
        # "accounted" = annotated, or deliberately left shared (const, address-taken,
        # name-excluded). Anything else that nm says is writable is a real miss.
        shared = (const_skipped_syms | address_taken_syms | exclude_syms
                  | macro_skipped | decl_only_skipped | macro_file_skipped)
        annotated_norm = {normalize(s) for s in annotated_syms} | annotated_syms
        shared_norm = {normalize(s) for s in shared} | shared

        dsda = [s for s in nm_syms if normalize(s) in seen_norm or s in seen_root_syms]
        thirdparty = [s for s in nm_syms if s not in dsda]
        genuine = sorted(s for s in dsda
                         if normalize(s) not in annotated_norm
                         and normalize(s) not in shared_norm)
        print(f"[deglob] nm writable symbols: {len(nm_syms)}  (DSDA-core: {len(dsda)}  "
              f"third-party-ignored: {len(thirdparty)})")
        print(f"[deglob] annotated: {len(annotated_syms)}  shared (const/addr-taken/excluded): {len(shared)}")
        print(f"[deglob] GENUINELY MISSED (DSDA-core, writable, mutable, not annotated): {len(genuine)}")
        for s in genuine:
            print("           ", s)

    if not args.apply:
        print("[deglob] dry-run (no files written). Re-run with --apply.")
        return

    for fpath, ins in edits.items():
        data = open(fpath, "rb").read()
        for offset, rlen, text in sorted(ins, key=lambda x: -x[0]):  # descending: offsets stay valid
            data = data[:offset] + text.encode() + data[offset + rlen:]
        open(fpath, "wb").write(data)
    print(f"[deglob] applied {total_edits} insertions across {len(edits)} files.")


if __name__ == "__main__":
    main()
