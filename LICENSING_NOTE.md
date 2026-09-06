# Licensing position (private build)

Recorded so that a later decision to release is made deliberately.

## What each input is under

| Component | Licence stated in tree | Notes |
|---|---|---|
| Spearmint engine (`clover-moe/spearmint`) | GPLv3 or later, **with the id Software additional terms** (`COPYING.txt` line 625 onwards: the RTCW / ET / Doom 3 style exception text) | README asks contributors to also grant GPLv2-or-later so the project could move to GPLv2 in future, but the shipped licence today is the modified GPLv3. |
| mint-arena game code | Same modified GPLv3 | Same reasoning; contains RTCW and ET code. |
| spearmint-patch-data | Mixed: shaders and icons from id Software / respective owners; Liberation fonts under SIL OFL 1.1; M+ fonts under the M+ licence | Only `fonts/` and `fallback-data/` would ship; the rest is per-game glue. |
| ioQuake3-wii donor port (`Mayo1970/ioQuake3-wii`) | `LICENSE.txt` is GPLv2. The vendored ioq3 engine code inside it is GPLv2 (id Software's Quake III licence) | Written permission from the author to reuse the platform layer has been obtained (per the brief). That permission covers the author's own code; it cannot relicense the vendored id/ioq3 code, which is GPLv2 already. |
| ioquake3 upstream | GPLv2 (Quake III Arena source licence, no "or later") | Reference only in this project. |
| libogc, libfat, devkitPPC runtime | libogc: permissive (zlib style / BSD style per file); devkitPPC newlib: BSD style | No copyleft interaction. |
| OpenArena data (test content) | GPLv2 (game data) | Test content only; not part of a release. |
| Quake III Arena data (test content) | Proprietary id Software | Test content only; never redistributable. |

## The conflict, stated once

GPLv2-only code (ioquake3 and the donor's port layer as licensed) cannot be
combined into one distributed program with GPLv3-or-later code (Spearmint,
mint-arena) unless the GPLv2 code is also available under a later version.
id Software's Quake III licence is GPLv2 without the "or later" clause.
Spearmint itself already contains id Tech 3 code and ships it as modified
GPLv3 on the basis of id's later relicensing of RTCW/ET/Doom 3 under GPLv3
with additional terms, and of ioquake3 code being contributed to Spearmint
under GPLv2-or-later by its authors. That is Spearmint's problem to defend,
not ours, as long as we take the engine from Spearmint.

The specific issue this project creates is the **donor port layer**: about
6,500 lines under `code/sys`, `code/renderer`, `code/input`, `code/audio`,
plus about 2,500 lines of patches to engine files. Those are GPLv2 by the
donor's `LICENSE.txt`. Linking them into a Spearmint binary and distributing
the result would need either:

1. the donor author's permission to use that code under GPLv2-or-later (or
   GPLv3-or-later), which is a one-line email given the permission already
   granted for private use; or
2. rewriting the port layer, which is small enough that option 1 is the only
   sensible path.

Note that the donor's `vm_powerpc.c` changes (JIT arena allocator, cache
flush) and its renderer choke-point patches modify ioq3 files, so they
inherit GPLv2 from the files they patch; the author can only relicense their
own diff. Since Spearmint carries those same ioq3 files under its own
licence stack, re-applying the donor's diff to Spearmint's copies is the
practical route and keeps the question confined to the donor's own lines.

## Why this is not a blocker now

GPL obligations attach to distribution (GPLv2 section 3, GPLv3 section 6).
This build is private, not distributed, and loads no untrusted content.
Nothing needs to happen now.

## What to do before any release

1. Ask the donor author for GPLv2-or-later (or GPLv3-or-later) on their port
   layer and patch set. Keep the reply with the repository.
2. Decide whether Spearmint's "GPLv3 plus additional terms" is acceptable for
   the release; if not, this project cannot ship on Spearmint at all, and the
   decision belongs before content work, not after.
3. Ship no Quake III Arena or OpenArena assets. Original content only, with
   its own licence file.
4. Provide the complete corresponding source (engine, game QVM source, build
   scripts, patched libogc if any) with the release.
5. Do a file-by-file header audit at that point, not before.

Nothing above is legal advice; it is the engineering record of what the
trees say about themselves.
