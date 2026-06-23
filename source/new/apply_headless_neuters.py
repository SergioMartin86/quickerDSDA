#!/usr/bin/env python3
"""
Apply physics-neutral source neuters to kept files of the minimal headless core.

Some kept files contain functions that only set up display/audio resources (no
effect on the simulation/physics state that the savestate hash captures), but
which crash once their subsystem's data tables are removed. We early-return them.
This is applied after rsync (rsync --delete wipes these), so it must be idempotent.

Each neuter inserts `return;` immediately after a function's opening brace.
"""
import os
import re
import sys

# (relative path under core/prboom2/src, function signature regex, reason)
NEUTERS = [
    ("r_data.c", r"void\s+R_PrecacheLevel\s*\(\s*void\s*\)",
     "precache only loads rendering graphics lumps; physics-neutral"),
    ("r_things.c", r"void\s+R_InitSprites\s*\(\s*const\s+char[^)]*\)",
     "sprite frame tables + render clip arrays are pure rendering"),
]

MARK = "/* [min-headless neuter] */ return;"


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "core/prboom2/src")
    applied = 0
    for rel, sig, reason in NEUTERS:
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            print(f"[neuter] skip (missing): {rel}", file=sys.stderr)
            continue
        src = open(path).read()
        # find "<sig> ... {" and insert the marker right after that opening brace
        m = re.search(sig + r"\s*\{", src)
        if not m:
            print(f"[neuter] WARNING: signature not found in {rel}", file=sys.stderr)
            continue
        brace = m.end()
        if src[brace:brace + 120].find(MARK) != -1:
            continue  # already applied (idempotent)
        src = src[:brace] + "\n  " + MARK + "  /* " + reason + " */\n" + src[brace:]
        open(path, "w").write(src)
        applied += 1
    print(f"[neuter] applied {applied} source neuter(s)")


if __name__ == "__main__":
    main()
