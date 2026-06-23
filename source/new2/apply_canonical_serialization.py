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
    # 8. Design B incr.3b: O(1) bulk teardown of the previous state on load
    s = s.replace(
        "  // remove all the current thinkers\n"
        "  for (th = thinkercap.next; th != &thinkercap; )\n"
        "  {\n"
        "    thinker_t *next = th->next;\n"
        "    if (P_IsMobjThinker(th))\n"
        "    {\n"
        "      P_RemoveMobj ((mobj_t *) th);\n"
        "      P_RemoveThinkerDelayed(th); // fix mobj leak\n"
        "    }\n"
        "    else\n"
        "      Z_Free (th);\n"
        "    th = next;\n"
        "  }\n"
        "  P_InitThinkers ();",
        "  // Increment 3b: bulk teardown of the previous state's thinkers. All dynamic\n"
        "  // objects (mobjs, special thinkers, and the secnode pools) live in the\n"
        "  // contiguous thinker arena, so the old state is discarded in O(1) by clearing\n"
        "  // every pointer into the arena and resetting it -- no per-object walk, no\n"
        "  // per-object malloc free. This replaces the per-object P_RemoveMobj loop; its\n"
        "  // only observable side effect not reproduced here is the itemrespawnque push,\n"
        "  // which matters solely in respawn modes (not targeted by this core).\n"
        "  {\n"
        "    extern __STORAGE_MODIFIER mobj_t **blocklinks;\n"
        "    extern __STORAGE_MODIFIER int      blocklinks_count;\n"
        "    void P_FreeSecNodeList(void);\n"
        "    int _i;\n"
        "\n"
        "    P_InitThinkers();                                  // clear the thinker + class lists\n"
        "    for (_i = 0; _i < numsectors; _i++)                // clear sector thing/secnode heads\n"
        "    {\n"
        "      sectors[_i].thinglist = NULL;\n"
        "      sectors[_i].touching_thinglist = NULL;\n"
        "    }\n"
        "    if (blocklinks)                                    // clear blockmap thing heads\n"
        "      memset(blocklinks, 0, (size_t) blocklinks_count * sizeof(*blocklinks));\n"
        "    P_FreeSecNodeList();                               // drop secnode pools (they live in the arena)\n"
        "    Z_ResetThinkerArena();                             // reclaim the whole slab in O(1)\n"
        "  }", 1)
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
    # Sector/line gameplay-state fields safe to omit at complevel 2 headless (per
    # per-field mutation/read analysis):
    #  sec->tag, li->tag, li->special_args : never mutated post level-load (constant).
    #  li->flags : only ML_MAPPED (renderer) mutates it -> constant when headless.
    #  li->player_activations : write-only statistics counter, never read by sim.
    # NOTE: sec->lightlevel is render-only too, but the deep-equivalence oracle
    # (getDeepStateHash) hashes it, so it is kept saved (like mobj sprite/frame).
    "sec->tag",
    "li->flags", "li->tag", "li->player_activations", "li->special_args",
]


def patch_trims(path):
    s = open(path).read()
    if "[min-headless trim]" in s:
        return False
    n = 0
    for fld in TRIM_FIELDS:
        # X for ints/fixed, BYTE for byte fields, ARRAY for fixed arrays — a field
        # matches exactly one save macro + its load counterpart; the rest no-op.
        for macro in ("P_SAVE_X", "P_LOAD_X", "P_SAVE_BYTE", "P_LOAD_BYTE",
                      "P_SAVE_ARRAY", "P_LOAD_ARRAY"):
            # indent-agnostic: sector/line fields are at 4 spaces, sidedef at 8.
            pat = re.compile(r"^(\s*)" + re.escape(f"{macro}({fld});") + r"\s*$", re.M)
            s, c = pat.subn(
                lambda m: f"{m.group(1)}// [min-headless trim] {macro}({fld});  "
                          f"// render-only/constant at headless Doom2 cl2", s, count=1)
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


ARENA_BLOCK = '''
/* ---- Increment 3: contiguous per-thread "thinker" arena ----
 * Dynamic level objects (mobjs + the special thinkers) are bump-allocated from
 * a single contiguous per-thread slab with a segregated (per exact 16-byte size
 * class) free list, so the whole live dynamic set occupies one contiguous
 * region. That contiguity is the foundation for the bulk zone-snapshot save/load
 * path (a single memcpy + pointer relocation in place of the per-thinker field
 * walk). Routing is gated by dsda_thinker_arena_active, flipped on in
 * P_SetupLevel *after* the one-time map geometry is loaded and off before it,
 * so only the dynamic objects (never the static geometry) land in the slab.
 * Blocks keep the ordinary memblock_t header and blockbytag linkage, so the
 * existing pointer-swizzling archive path works unchanged on top of this. */
#ifndef DSDA_THINKER_ARENA_BYTES
#define DSDA_THINKER_ARENA_BYTES ((size_t)32 * 1024 * 1024)
#endif
#define TA_ALIGN       16u
#define TA_MAX_CLASSES 2048u   /* arena-eligible up to TA_MAX_CLASSES*TA_ALIGN = 32KB; larger falls back to malloc */

__STORAGE_MODIFIER int dsda_thinker_arena_active;       /* routing flag (set in p_setup.c) */
static __STORAGE_MODIFIER char       *ta_base;          /* slab base (malloc'd once per thread) */
static __STORAGE_MODIFIER size_t      ta_high;          /* bump high-water mark (bytes used) */
static __STORAGE_MODIFIER size_t      ta_peak;          /* max high-water seen (for reporting/sizing) */
static __STORAGE_MODIFIER memblock_t *ta_free[TA_MAX_CLASSES]; /* segregated free lists, keyed by size class */

/* Reclaim the whole slab in O(1): drop the bump pointer and all free lists. The
 * caller must have already cleared every pointer into the slab (thinker list,
 * sector/blockmap heads, block-zone pools). Used by the bulk state-load teardown
 * to abandon the previous state's objects without walking them. The routing flag
 * is left untouched so subsequent allocations keep using the slab. */
void Z_ResetThinkerArena(void)
{
  ta_high = 0;
  memset(ta_free, 0, sizeof(ta_free));
}

/* Reset the slab to empty and start routing dynamic allocations into it. Called
 * after the geometry load, when no thinker objects are live. */
void Z_BeginThinkerArena(void)
{
  Z_ResetThinkerArena();
  dsda_thinker_arena_active = 1;
}

/* Stop routing into the slab (e.g. while (re)loading static geometry). */
void Z_EndThinkerArena(void)
{
  dsda_thinker_arena_active = 0;
}

size_t Z_ThinkerArenaUsed(void) { return ta_high; }
size_t Z_ThinkerArenaPeak(void) { return ta_peak; }

static memblock_t *Z_ArenaCarve(size_t total, unsigned cls)
{
  memblock_t *b;
  if (ta_free[cls])                 /* exact-size reuse: perfect for fixed-size structs */
  {
    b = ta_free[cls];
    ta_free[cls] = b->next;
    return b;
  }
  if (!ta_base)
  {
    ta_base = malloc(DSDA_THINKER_ARENA_BYTES);
    if (!ta_base)
      I_Error("Z_ArenaCarve: failed to reserve %lu-byte thinker arena", (unsigned long) DSDA_THINKER_ARENA_BYTES);
  }
  if (ta_high + total > DSDA_THINKER_ARENA_BYTES)
    I_Error("Z_ArenaCarve: thinker arena exhausted (need %lu)", (unsigned long)(ta_high + total));
  b = (memblock_t *)(ta_base + ta_high);
  ta_high += total;
  if (ta_high > ta_peak) ta_peak = ta_high;
  return b;
}

static inline int Z_InArena(const void *block)
{
  return ta_base && (const char *)block >= ta_base && (const char *)block < ta_base + ta_high;
}
'''


def patch_zone(zone_c, zone_h):
    s = open(zone_c).read()
    if "dsda_thinker_arena_active" in s:
        return False
    # 1. arena globals + helpers, right after the per-tag block list
    s = s.replace(
        "static __STORAGE_MODIFIER memblock_t *blockbytag[ZONE_MAX];\n",
        "static __STORAGE_MODIFIER memblock_t *blockbytag[ZONE_MAX];\n" + ARENA_BLOCK, 1)
    # 2. route dynamic level allocations into the arena
    s = s.replace(
        "  memblock_t *block = NULL;\n"
        "\n"
        "  if (!size)\n"
        "    return NULL; // malloc(0) returns NULL\n"
        "\n"
        "  if (!(block = malloc(size + HEADER_SIZE)))\n"
        "  {\n"
        "    I_Error (\"Z_Malloc: Failure trying to allocate %lu bytes\", (unsigned long) size);\n"
        "  }",
        "  memblock_t *block = NULL;\n"
        "  size_t blocksize = size;\n"
        "\n"
        "  if (!size)\n"
        "    return NULL; // malloc(0) returns NULL\n"
        "\n"
        "  // Increment 3: route dynamic level objects into the contiguous thinker arena.\n"
        "  if (tag == ZONE_LEVEL && dsda_thinker_arena_active)\n"
        "  {\n"
        "    size_t rounded = (size + (TA_ALIGN - 1)) & ~(size_t)(TA_ALIGN - 1);\n"
        "    unsigned cls = (unsigned)(rounded / TA_ALIGN);\n"
        "    if (cls < TA_MAX_CLASSES)\n"
        "    {\n"
        "      block = Z_ArenaCarve(rounded + HEADER_SIZE, cls);\n"
        "      blocksize = rounded;   // capacity == class size, so re-free maps back to the same class\n"
        "    }\n"
        "  }\n"
        "\n"
        "  if (!block && !(block = malloc(blocksize + HEADER_SIZE)))\n"
        "  {\n"
        "    I_Error (\"Z_Malloc: Failure trying to allocate %lu bytes\", (unsigned long) size);\n"
        "  }", 1)
    # 3. record the (possibly rounded) capacity as the block size
    s = s.replace(
        "  block->size = size;\n"
        "  block->signature = ZONE_SIGNATURE;",
        "  block->size = blocksize;\n"
        "  block->signature = ZONE_SIGNATURE;", 1)
    # 4. arena blocks return to their size-class free list, not the heap
    s = s.replace(
        "  block->prev->next = block->next;\n"
        "  block->next->prev = block->prev;\n"
        "\n"
        "  free(block);\n"
        "}",
        "  block->prev->next = block->next;\n"
        "  block->next->prev = block->prev;\n"
        "\n"
        "  // Increment 3: arena blocks return to their size-class free list, not the heap.\n"
        "  if (Z_InArena(block))\n"
        "  {\n"
        "    unsigned cls = (unsigned)(block->size / TA_ALIGN);\n"
        "    block->next = ta_free[cls];\n"
        "    ta_free[cls] = block;\n"
        "    return;\n"
        "  }\n"
        "\n"
        "  free(block);\n"
        "}", 1)
    open(zone_c, "w").write(s)
    # header decls
    h = open(zone_h).read()
    if "Z_BeginThinkerArena" not in h:
        h = h.replace(
            "char *Z_StrdupLevel(const char *s);\n",
            "char *Z_StrdupLevel(const char *s);\n\n"
            "/* Increment 3: contiguous per-thread thinker arena (see z_zone.c) */\n"
            "void   Z_BeginThinkerArena(void);\n"
            "void   Z_EndThinkerArena(void);\n"
            "void   Z_ResetThinkerArena(void);\n"
            "size_t Z_ThinkerArenaUsed(void);\n"
            "size_t Z_ThinkerArenaPeak(void);\n", 1)
        open(zone_h, "w").write(h)
    return True


def patch_setup(path):
    s = open(path).read()
    if "Z_BeginThinkerArena" in s:
        return False
    s = s.replace(
        "  //e6y\n"
        "  totallive = 0;\n"
        "\n"
        "  main_tranmap = dsda_DefaultTranMap();",
        "  //e6y\n"
        "  totallive = 0;\n"
        "\n"
        "  // Increment 3: static map geometry is loaded below with the thinker arena\n"
        "  // OFF, so only the dynamic objects (spawned from load_things onward) land in\n"
        "  // the contiguous slab.\n"
        "  Z_EndThinkerArena();\n"
        "\n"
        "  main_tranmap = dsda_DefaultTranMap();", 1)
    s = s.replace(
        "  map_loader.load_things(level_components.things);",
        "  // Increment 3: geometry is loaded; route all dynamic objects spawned from\n"
        "  // here on (initial things, specials, and all gameplay spawns) into the slab.\n"
        "  Z_BeginThinkerArena();\n"
        "\n"
        "  map_loader.load_things(level_components.things);", 1)
    open(path, "w").write(s)
    return True


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "core/prboom2/src")
    a = patch_psaveg(os.path.join(root, "p_saveg.c"))
    t = patch_trims(os.path.join(root, "p_saveg.c"))
    b = patch_save(os.path.join(root, "dsda/save.c"))
    m = patch_maputl(os.path.join(root, "p_maputl.c"))
    z = patch_zone(os.path.join(root, "z_zone.c"), os.path.join(root, "z_zone.h"))
    p = patch_setup(os.path.join(root, "p_setup.c"))
    print(f"[canonical] p_saveg.c {'patched' if a else 'already-applied'}, "
          f"trims {t if t is not False else 'already-applied'}, "
          f"save.c {'patched' if b else 'already-applied'}, "
          f"p_maputl.c {'patched' if m else 'already-applied'}, "
          f"z_zone.c {'patched' if z else 'already-applied'}, "
          f"p_setup.c {'patched' if p else 'already-applied'}")


if __name__ == "__main__":
    main()
