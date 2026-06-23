#!/usr/bin/env python3
"""
Design A: make the savestate byte-canonical.

The archive copies whole structs, so allocation-dependent pointer fields (and one
gametic-relative counter) land in the serialized bytes verbatim -- identical game
states then serialize to *different* bytes across threads/branches. Every such
field is rebuilt/re-derived on load, so we overwrite the buffer copy with
deterministic values. This makes states byte-comparable (dedup) and is the
prerequisite for a raw zone-snapshot. Verified by `tTester2 --strict`.

Applied to the kept files p_saveg.c + dsda/save.c after rsync+deglobalize (rsync
--delete wipes them), so it must be idempotent. Run from regenerate.sh.
"""
import os
import re
import sys

HELPERS = """\
// ---- Design A: canonical serialization (apply_canonical_serialization.py) ----
// Overwrite allocation-dependent pointer fields in the *buffer copy* (never the
// live object) with deterministic values. Everything zeroed here is rebuilt or
// re-derived on load (P_AddThinker / P_SetThingPosition / &mobjinfo[type] / ...).

static void P_CanonicalizeMobj(mobj_t *d)
{
  d->thinker.next = NULL;                 // thinker.prev holds the swizzled index
  d->thinker.cnext = NULL;
  d->thinker.cprev = NULL;
  d->thinker.function = NULL;             // load sets P_MobjThinker unconditionally
  d->snext = NULL; d->sprev = NULL;       // sector/blockmap links: P_SetThingPosition
  d->bnext = NULL; d->bprev = NULL;
  /* subsector swizzled to an index in the mobj archive (Design B incr.1), not nulled */
  d->touching_sectorlist = NULL;
  d->info = NULL;                         // re-derived: &mobjinfo[type]
  d->tranmap = NULL;                      // re-derived from alpha
  d->PrevX = 0; d->PrevY = 0; d->PrevZ = 0;  // render interpolation (reset each tic), removes diff noise
}

static void P_CanonicalizeSpecialThinker(thinker_t *d)
{
  // List links rebuilt by P_AddThinker; function restored from the tc_* class byte,
  // but ceiling/plat read NULL-vs-set as a stasis flag, so collapse to a 0/1 sentinel.
  d->prev = NULL;
  d->next = NULL;
  d->cnext = NULL;
  d->cprev = NULL;
  d->function = d->function ? (think_t)(intptr_t)1 : NULL;
}

void P_ArchivePlayers (void)
"""

PLAYERS_OLD = """\
            dest->psprites[j].state =
              (state_t *)(dest->psprites[j].state-states);
      }
}"""

PLAYERS_NEW = """\
            dest->psprites[j].state =
              (state_t *)(dest->psprites[j].state-states);

        // Design A: canonicalize raw mobj pointers (all NULL'd / re-linked on load)
        dest->mo = NULL;
        dest->attacker = NULL;
        dest->rain1 = NULL;
        dest->rain2 = NULL;
        dest->poisoner = NULL;
      }
}"""

MOBJ_OLD = """\
        mobj->player = (player_t *)((mobj->player-players) + 1);
    }
  }

  // add a terminating marker
  P_SAVE_BYTE(tc_end);"""

MOBJ_NEW = """\
        mobj->player = (player_t *)((mobj->player-players) + 1);

      P_CanonicalizeMobj(mobj);   // Design A: zero remaining raw pointer bytes
    }
  }

  // add a terminating marker
  P_SAVE_BYTE(tc_end);"""

CTX_SAVE_OLD = """\
  boom_logictic_value = boom_logictic;
  P_SAVE_X(boom_logictic_value);

  true_logictic_value = true_logictic;
  P_SAVE_X(true_logictic_value);"""

CTX_SAVE_NEW = """\
  // Design A: save boom/true *basetic* directly (canonical state). The derived
  // boom_logictic (= gametic - basetic) was reconstructed on load from the stale
  // pre-restore gametic, making the bytes depend on the loading thread's history.
  (void) boom_logictic_value;
  (void) true_logictic_value;
  P_SAVE_X(boom_basetic);
  P_SAVE_X(true_basetic);"""

CTX_LOAD_OLD = """\
  P_LOAD_X(boom_logictic_value);
  boom_basetic = gametic - boom_logictic_value;

  P_LOAD_X(true_logictic_value);
  true_basetic = gametic - true_logictic_value;"""

CTX_LOAD_NEW = """\
  // Design A: load boom/true basetic directly (no stale-gametic reconstruction).
  (void) boom_logictic_value;
  (void) true_logictic_value;
  P_LOAD_X(boom_basetic);
  P_LOAD_X(true_basetic);"""


def patch_psaveg(path):
    s = open(path).read()
    if "P_CanonicalizeMobj" in s:
        return False  # already applied
    # 1. helpers (the HELPERS block ends with the P_ArchivePlayers signature line)
    s = s.replace("void P_ArchivePlayers (void)\n", HELPERS, 1)
    # 2. player raw-pointer zeroing
    s = s.replace(PLAYERS_OLD, PLAYERS_NEW, 1)
    # 3. canonicalize every non-mobj thinker header right after its struct copy
    def ref(m):
        var, typ = m.group(1), m.group(2)
        if typ == "mobj_t":
            return m.group(0)
        return m.group(0) + f"\n      P_CanonicalizeSpecialThinker((thinker_t*){var});"
    s = re.sub(r"P_SAVE_TYPE_REF\(th,\s*(\w+),\s*(\w+)\);", ref, s)
    s = re.sub(r"P_SAVE_TYPE\(th,\s*(\w+)\);",
               r"{ \1* _cz; P_SAVE_TYPE_REF(th, _cz, \1); P_CanonicalizeSpecialThinker((thinker_t*)_cz); }", s)
    # 4. per-type re-derived back-pointers
    s = s.replace(
        "ceiling->sector = (sector_t *)(intptr_t)(ceiling->sector->iSectorID);",
        "ceiling->sector = (sector_t *)(intptr_t)(ceiling->sector->iSectorID);\n"
        "      ceiling->list = NULL; /* re-created on load by P_AddActiveCeiling */", 1)
    s = s.replace(
        "plat->sector = (sector_t *)(intptr_t)(plat->sector->iSectorID);",
        "plat->sector = (sector_t *)(intptr_t)(plat->sector->iSectorID);\n"
        "      plat->list = NULL; /* re-created on load by P_AddActivePlat */", 1)
    s = s.replace(
        "{ pusher_t* _cz; P_SAVE_TYPE_REF(th, _cz, pusher_t); P_CanonicalizeSpecialThinker((thinker_t*)_cz); }",
        "{ pusher_t* _cz; P_SAVE_TYPE_REF(th, _cz, pusher_t); P_CanonicalizeSpecialThinker((thinker_t*)_cz);\n"
        "        _cz->source = NULL; /* re-derived on load via P_GetPushThing(affectee) */ }", 1)
    # 5. mobj canonicalize call
    s = s.replace(MOBJ_OLD, MOBJ_NEW, 1)
    # 6. Design B incr.1: subsector cache (skip R_PointInSubsector on load)
    s = s.replace(
        "__STORAGE_MODIFIER byte *save_p;",
        "extern __STORAGE_MODIFIER int dsda_use_saved_subsector;  /* Design B incr.1 (p_maputl.c) */\n"
        "__STORAGE_MODIFIER byte *save_p;", 1)
    s = s.replace(
        "      mobj->state = (state_t *)(mobj->state - states);",
        "      mobj->state = (state_t *)(mobj->state - states);\n"
        "      mobj->subsector = (subsector_t *)(intptr_t)(mobj->subsector - subsectors); /* incr.1 */", 1)
    s = s.replace(
        "          mobj->state = states + (intptr_t) mobj->state;",
        "          mobj->state = states + (intptr_t) mobj->state;\n"
        "          mobj->subsector = subsectors + (intptr_t) mobj->subsector; /* incr.1 */", 1)
    s = s.replace(
        "          P_SetThingPosition (mobj);",
        "          dsda_use_saved_subsector = 1; /* incr.1: trust restored subsector */\n"
        "          P_SetThingPosition (mobj);\n"
        "          dsda_use_saved_subsector = 0;", 1)
    # 7. Design B incr.2a: save/rebuild touching_sectorlist (skip P_CreateSecNodeList)
    s = s.replace(
        "extern __STORAGE_MODIFIER int dsda_use_saved_subsector;  /* Design B incr.1 (p_maputl.c) */\n",
        "extern __STORAGE_MODIFIER int dsda_use_saved_subsector;  /* Design B incr.1 (p_maputl.c) */\n"
        "extern __STORAGE_MODIFIER int dsda_skip_secnode_build;   /* Design B incr.2a (p_maputl.c) */\n"
        "extern msecnode_t* P_AddSecnode(sector_t* s, mobj_t* thing, msecnode_t* nextnode);  /* incr.2a: 64-bit return */\n", 1)
    # 7a. SAVE: append the thing's touching-sector indices (list order)
    s = s.replace(
        "      P_CanonicalizeMobj(mobj);   // Design A: zero remaining raw pointer bytes\n",
        "      P_CanonicalizeMobj(mobj);   // Design A: zero remaining raw pointer bytes\n"
        "      { /* incr.2a: save touching-sector indices to skip the blockmap scan on load */\n"
        "        mobj_t *live = (mobj_t *)th; const msecnode_t *m; int count = 0;\n"
        "        for (m = live->touching_sectorlist; m; m = m->m_tnext) count++;\n"
        "        P_SAVE_X(count);\n"
        "        for (m = live->touching_sectorlist; m; m = m->m_tnext) { int si = (int)(m->m_sector - sectors); P_SAVE_X(si); }\n"
        "      }\n", 1)
    # 7b. count pre-pass: skip the variable-length sec list after each tc_mobj
    s = s.replace(
        "        tc == tc_mobj           ? sizeof(mobj_t)           :\n"
        "      0;\n    }",
        "        tc == tc_mobj           ? sizeof(mobj_t)           :\n"
        "      0;\n"
        "      if (tc == tc_mobj) { int sc; memcpy(&sc, save_p, sizeof(int)); save_p += sizeof(int) + (size_t) sc * sizeof(int); } /* incr.2a */\n    }", 1)
    # 7c. LOAD: read the saved sec indices (before the marked-for-deletion break)
    s = s.replace(
        "          mobj->subsector = subsectors + (intptr_t) mobj->subsector; /* incr.1 */\n",
        "          mobj->subsector = subsectors + (intptr_t) mobj->subsector; /* incr.1 */\n"
        "          int sec_count; int sec_idx[256]; /* incr.2a */\n"
        "          P_LOAD_X(sec_count);\n"
        "          if (sec_count < 0 || sec_count > 256) I_Error(\"P_UnArchiveThinkers: bad touching-sector count %d\", sec_count);\n"
        "          { int _i; for (_i = 0; _i < sec_count; _i++) P_LOAD_X(sec_idx[_i]); }\n", 1)
    # 7d. LOAD: skip P_CreateSecNodeList + rebuild touching_sectorlist from saved indices
    s = s.replace(
        "          dsda_use_saved_subsector = 1; /* incr.1: trust restored subsector */\n"
        "          P_SetThingPosition (mobj);\n"
        "          dsda_use_saved_subsector = 0;",
        "          dsda_use_saved_subsector = 1; /* incr.1 */\n"
        "          dsda_skip_secnode_build = 1;  /* incr.2a */\n"
        "          P_SetThingPosition (mobj);\n"
        "          dsda_skip_secnode_build = 0;\n"
        "          dsda_use_saved_subsector = 0;\n"
        "          { msecnode_t *sl = NULL; int _i; /* incr.2a: rebuild touching_sectorlist (reverse preserves order) */\n"
        "            for (_i = sec_count - 1; _i >= 0; _i--) sl = P_AddSecnode(&sectors[sec_idx[_i]], mobj, sl);\n"
        "            mobj->touching_sectorlist = sl; }", 1)
    open(path, "w").write(s)
    return True


def patch_maputl(path):
    s = open(path).read()
    if "dsda_use_saved_subsector" in s:
        return False
    s = s.replace(
        "void P_SetThingPosition(mobj_t *thing)\n"
        "{                                                      // link into subsector\n"
        "  subsector_t *ss = thing->subsector = R_PointInSubsector(thing->x, thing->y);",
        "// Design B incr.1: when set, trust the caller-provided thing->subsector\n"
        "// (restored from the savestate) instead of recomputing it via R_PointInSubsector\n"
        "// (~20% of state-load time). subsector == R_PointInSubsector(x,y) for any\n"
        "// positioned thing, so this is exact. The savestate loader sets it around its\n"
        "// relink calls only.\n"
        "__STORAGE_MODIFIER int dsda_use_saved_subsector = 0;\n\n"
        "void P_SetThingPosition(mobj_t *thing)\n"
        "{                                                      // link into subsector\n"
        "  subsector_t *ss = thing->subsector =\n"
        "    dsda_use_saved_subsector ? thing->subsector : R_PointInSubsector(thing->x, thing->y);", 1)
    # Design B incr.2a: flag to skip P_CreateSecNodeList (the blockmap line scan);
    # the loader rebuilds touching_sectorlist directly from saved sector indices.
    s = s.replace(
        "__STORAGE_MODIFIER int dsda_use_saved_subsector = 0;\n",
        "__STORAGE_MODIFIER int dsda_use_saved_subsector = 0;\n"
        "__STORAGE_MODIFIER int dsda_skip_secnode_build = 0;  // Design B incr.2a\n", 1)
    s = s.replace(
        "      P_CreateSecNodeList(thing,thing->x,thing->y);\n"
        "      thing->touching_sectorlist = sector_list; // Attach to Thing's mobj_t\n"
        "      sector_list = NULL; // clear for next time",
        "      if (!dsda_skip_secnode_build)\n"
        "      {\n"
        "        P_CreateSecNodeList(thing,thing->x,thing->y);\n"
        "        thing->touching_sectorlist = sector_list; // Attach to Thing's mobj_t\n"
        "        sector_list = NULL; // clear for next time\n"
        "      }", 1)
    open(path, "w").write(s)
    return True


# Phase 5: drop sector/line fields that are ZDoom/UDMF-only and therefore constant
# for Doom/Doom2 maps (never written by Doom2 gameplay). Trimmed symmetrically from
# P_ArchiveWorld + P_UnArchiveWorld. Safe with the G_InitNew skip because the value
# is invariant for the level. NOT trimming standard sidedef textures/offsets --
# switches (P_ChangeSwitchTexture) and scrolling walls mutate those.
TRIM_FIELDS = [
    # ZDoom/UDMF-only sector + line fields: constant for Doom2 (never written).
    "sec->gravity", "sec->damage", "sec->lightlevel_floor", "sec->lightlevel_ceiling",
    "sec->floor_rotation", "sec->ceiling_rotation", "sec->floor_xscale", "sec->floor_yscale",
    "sec->ceiling_xscale", "sec->ceiling_yscale", "sec->floor_xoffs", "sec->floor_yoffs",
    "sec->ceiling_xoffs", "sec->ceiling_yoffs", "sec->seqType",
    "li->automap_style", "li->health", "li->alpha",
    # Sidedef textures/offsets: strictly graphical (render-only; collision is geometry).
    # Switches/scrolling mutate them but that is purely visual, never physics.
    "si->textureoffset", "si->rowoffset", "si->toptexture", "si->bottomtexture", "si->midtexture",
]


def patch_trims(path):
    s = open(path).read()
    if "[min-headless trim]" in s:
        return False
    n = 0
    for fld in TRIM_FIELDS:
        for macro in ("P_SAVE_X", "P_LOAD_X"):
            # indent-agnostic: sector/line fields are at 4 spaces, sidedef at 8.
            pat = re.compile(r"^(\s*)" + re.escape(f"{macro}({fld});") + r"\s*$", re.M)
            s, c = pat.subn(
                lambda m: f"{m.group(1)}// [min-headless trim] {macro}({fld});  "
                          f"// render-only/zdoom field, irrelevant to Doom2 physics", s, count=1)
            n += c
    open(path, "w").write(s)
    return n


def patch_save(path):
    s = open(path).read()
    if "P_SAVE_X(boom_basetic)" in s:
        return False
    s = s.replace(CTX_SAVE_OLD, CTX_SAVE_NEW, 1)
    s = s.replace(CTX_LOAD_OLD, CTX_LOAD_NEW, 1)
    open(path, "w").write(s)
    return True


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "core/prboom2/src")
    a = patch_psaveg(os.path.join(root, "p_saveg.c"))
    t = patch_trims(os.path.join(root, "p_saveg.c"))
    b = patch_save(os.path.join(root, "dsda/save.c"))
    m = patch_maputl(os.path.join(root, "p_maputl.c"))
    print(f"[canonical] p_saveg.c {'patched' if a else 'already-applied'}, "
          f"trims {t if t is not False else 'already-applied'}, "
          f"save.c {'patched' if b else 'already-applied'}, "
          f"p_maputl.c {'patched' if m else 'already-applied'}")


if __name__ == "__main__":
    main()
