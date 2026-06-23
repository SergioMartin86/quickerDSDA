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
  d->subsector = NULL;
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
    print(f"[canonical] p_saveg.c {'patched' if a else 'already-applied'}, "
          f"trims {t if t is not False else 'already-applied'}, "
          f"save.c {'patched' if b else 'already-applied'}")


if __name__ == "__main__":
    main()
