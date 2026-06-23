#!/usr/bin/env python3
"""
Generate the minimal-headless stub file (__headless_stubs.c) that satisfies the
references the kept simulation code makes into the removed display/sound/non-Doom
subsystems. Encapsulates the full reproducible recipe so the minimal core can be
rebuilt from pristine base in one command (via regenerate.sh):

  1. write an empty stub, build new2Tester -> capture undefined references
  2. gen_stubs.py over removed_files.txt (no-op funcs + zeroed data from the
     removed files' real signatures)
  3. resolve the data symbols gen_stubs can't locate, from their header
     extern-declarations (+ a small manual type map for awkward cases)
  4. build once to refresh objects; derive the authoritative thread-local set
     (readelf) and the set of symbols already defined in kept objects (nm)
  5. normalize: every stub data symbol gets __STORAGE_MODIFIER iff thread-local;
     drop any stub that duplicates a kept-object definition

The caller does the final build.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
QD = os.path.abspath(os.path.join(HERE, "..", ".."))
ROOT = os.path.join(QD, "source/new2/core/prboom2/src")
BUILD = os.path.join(QD, "build")
STUB = os.path.join(ROOT, "__headless_stubs.c")
CC = os.path.join(BUILD, "compile_commands.json")
REMOVED = os.path.join(HERE, "removed_files.txt")

# Headers the stub needs for the types used in removed-subsystem signatures.
INCLUDES = [
    "doomstat.h", "dsda.h", "dsda/mapinfo/doom/parser.h", "dsda/udmf.h",
    "dsda/mapinfo.h", "dsda/mapinfo/doom.h", "dsda/mapinfo/u.h", "dsda/utility.h",
    "dsda/text_file.h", "dsda/split_tracker.h", "d_main.h", "d_player.h",
    "p_inter.h", "p_user.h", "r_defs.h", "r_state.h", "r_main.h", "r_draw.h",
    "r_patch.h", "r_bsp.h", "r_plane.h", "r_segs.h", "r_sky.h", "p_spec.h",
    "p_mobj.h", "m_menu.h", "sounds.h", "s_sound.h", "s_advsound.h", "v_video.h",
    "st_stuff.h", "st_lib.h", "hu_stuff.h", "hu_lib.h", "am_map.h", "wi_stuff.h",
    "f_finale.h", "f_wipe.h", "dsda/exhud.h", "dsda/render_stats.h",
    "dsda/palette.h", "dsda/stretch.h", "umapinfo.h", "dsda/ambient.h", "m_cheat.h", "dsda/analysis.h", "dsda/ghost.h", "dsda/brute_force.h", "dsda/build.h", "dsda/tracker.h", "dsda/console.h", "dsda/messenger.h", "dsda/wad_stats.h", "dsda/episode.h", "md5.h",
    "hexen/po_man.h", "hexen/p_acs.h", "hexen/sn_sonix.h", "hexen/p_things.h",
    "hexen/in_lude.h", "hexen/f_finale.h", "hexen/p_anim.h", "hexen/a_action.h",
    "heretic/mn_menu.h", "heretic/f_finale.h", "heretic/in_lude.h",
    "heretic/sb_bar.h",
]

# Manual definitions for symbols whose extern decl is awkward (multi-declarator,
# pointer types the int-fallback would mis-size, no header decl).
MANUAL = {
    "PolyBlockMap": "polyblock_t** PolyBlockMap;",
    "Sky1ScrollDelta": "int Sky1ScrollDelta;", "Sky2ScrollDelta": "int Sky2ScrollDelta;",
    "mn_SuicideConsole": "int mn_SuicideConsole;",
    "main_tranmap": "const byte *main_tranmap;", "tranmap": "const byte *tranmap;",
    "walllights": "const lighttable_t **walllights;", "sprites": "spritedef_t *sprites;",
    "ratio_multiplier": "unsigned int ratio_multiplier;",
    "ratio_scale": "unsigned int ratio_scale;", "LightningFlash": "int LightningFlash;",
    "S_sfx": "sfxinfo_t* S_sfx;", "S_music": "musicinfo_t* S_music;",
    "num_sfx": "int num_sfx;", "idmusnum": "int idmusnum;", "musinfo": "musinfo_t musinfo;",
    "full_sounds": "int full_sounds;", "snd_SfxVolume": "int snd_SfxVolume;",
    "snd_samplerate": "int snd_samplerate;",
    "local_cmds": "__STORAGE_MODIFIER ticcmd_t local_cmds[MAX_MAXPLAYERS];",
    "dsda_TickElapsedTime": "__STORAGE_MODIFIER unsigned long long (*dsda_TickElapsedTime)(void);",
}


def _link_output():
    """Force a clean relink of new2Tester and return the combined build output."""
    try:
        os.remove(os.path.join(BUILD, "new2Tester"))
    except OSError:
        pass
    r = subprocess.run(["ninja", "-C", BUILD, "new2Tester"], capture_output=True, text=True)
    return r.stdout + r.stderr


def ninja_undefined():
    """Return the set of undefined-reference symbol names for new2Tester.

    Two-phase so it is robust on a *fresh* tree (no objects yet): the first build
    compiles every object and stops at the failing link; the second is a pure
    relink that reliably emits the full undefined-reference list. Union both."""
    out = _link_output() + "\n" + _link_output()
    return set(re.findall(r"undefined reference to `([A-Za-z_]\w*)'", out))


def header_text_and_deglob():
    text, deglob = [], set()
    pat = re.compile(r"__STORAGE_MODIFIER\b[^;{}=]*?([A-Za-z_]\w*)\s*(\[|;|=|,)")
    for dp, _, fns in os.walk(ROOT):
        for fn in fns:
            if fn.endswith((".h", ".hpp")):
                try:
                    t = open(os.path.join(dp, fn), errors="replace").read()
                except OSError:
                    continue
                text.append(t)
                deglob |= {m.group(1) for m in pat.finditer(t)}
    return "\n".join(text), deglob


def main():
    # 1. empty stub -> undefined references
    open(STUB, "w").write("/* placeholder */\n")
    subprocess.run(["meson", "setup", BUILD, "--reconfigure", "-DonlyFree=false"],
                   capture_output=True)
    undef = ninja_undefined()
    print(f"[stubs] undefined references: {len(undef)}")

    # 2. gen_stubs.py over the removed set
    inc = []
    for h in INCLUDES:
        inc += ["--include", h]
    undef_file = "/tmp/_headless_undef.txt"
    open(undef_file, "w").write("\n".join(sorted(undef)) + "\n")
    subprocess.run(["python3", os.path.join(HERE, "gen_stubs.py"),
                    "--undefined", undef_file, "--removed-list", REMOVED,
                    "--root", ROOT, "--compile-commands", CC, "--out", STUB] + inc,
                   check=True)
    # gen_stubs.py already emits the config.h + --include preamble at the top.

    # 3. resolve the data symbols gen_stubs could not locate
    stub_defined = set(re.findall(r"\b([A-Za-z_]\w*)\s*[\(;\[=]", open(STUB).read()))
    missing = sorted(s for s in undef if s not in stub_defined)
    text, deglob = header_text_and_deglob()
    resolved = []
    for s in missing:
        if s.startswith("__deglob_tls_init"):
            resolved.append(f"void {s}(void) {{ }}")
            continue
        if s in MANUAL:
            resolved.append(MANUAL[s])
            continue
        m = re.search(r"extern\s+((?:__STORAGE_MODIFIER\s+)?[A-Za-z_][\w ]*?[\*\s]*"
                      + re.escape(s) + r"\s*(?:\[[^\]]*\])*)\s*;", text)
        if m:
            decl = re.sub(r"^\s*__STORAGE_MODIFIER\s+", "", m.group(1)).strip()
            decl = re.sub(r"\[\s*\]", "[1]", decl)
            resolved.append(("__STORAGE_MODIFIER " if s in deglob else "") + decl + ";")
        else:
            resolved.append(f"int {s};")
    open(STUB, "a").write("\n/* resolved data + orphan inits */\n" + "\n".join(resolved) + "\n")

    # 4. build once to refresh objects, then derive TLS + kept-defined sets
    subprocess.run(["ninja", "-C", BUILD, "new2Tester"], capture_output=True)
    tls, kept = set(), set()
    import glob
    for o in glob.glob(os.path.join(BUILD, "new2Tester.p", "*.o")):
        if "___headless_stubs" in o:
            continue
        rel = subprocess.run(["readelf", "-sW", o], capture_output=True, text=True).stdout
        for line in rel.splitlines():
            p = line.split()
            if len(p) >= 8 and p[3] == "TLS":
                tls.add(p[7].split("@")[0])
        nm = subprocess.run(["nm", o], capture_output=True, text=True).stdout
        for line in nm.splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in "BbDdGgRrTtVvWw":
                kept.add(p[2])

    # 5. normalize TLS-ness + drop kept-object duplicates
    out = []
    for ln in open(STUB).read().split("\n"):
        st = ln.strip()
        if not st or st.startswith(("/*", "//", "#")):
            out.append(ln)
            continue
        fm = re.match(r".*\b([A-Za-z_]\w*)\s*\(.*\)\s*\{", ln)
        if fm:  # function stub
            if fm.group(1) not in kept:
                out.append(ln)
            continue
        bare = re.sub(r"^\s*__STORAGE_MODIFIER\s+", "", ln)
        dm = re.match(r".*?([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*(?:=\s*\{0\})?\s*;\s*$", bare)
        if dm:
            if dm.group(1) in kept:
                continue
            out.append(("__STORAGE_MODIFIER " + bare.lstrip()) if dm.group(1) in tls else bare)
            continue
        out.append(ln)
    open(STUB, "w").write("\n".join(out))
    print(f"[stubs] done: {sum(1 for l in out if l.strip().endswith('}'))} funcs, "
          f"TLS-normalized + kept-deduped.")


if __name__ == "__main__":
    main()
