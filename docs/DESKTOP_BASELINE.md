# Desktop baseline: Spearmint + mint-arena on Linux x86-64

This is the reference every later Wii measurement is judged against. It was
produced on 2026-09-06 in a headless container (8 vCPU x86-64, Mesa
llvmpipe software GL under Xvfb, no audio). The GPU-side number (`bk`) is
therefore a software rasteriser and is **not** comparable to GX; the CPU-side
numbers and all memory numbers are the useful part.

## Reproduce

```
sudo apt-get install build-essential libsdl2-dev libgl1-mesa-dev xvfb mesa-utils python3 unzip curl git
tools/desktop/setup.sh ~/desktop-baseline          # clones, builds, stages OpenArena 0.8.8 data
cd ~/desktop-baseline && mkdir -p bench
while read -r n m lp gh ch b; do tools/desktop/bench.sh ~/desktop-baseline "$n" "$m" "$lp" "$gh" "$ch" "$b"; done < tools/desktop/matrix.txt
tools/desktop/summarize.py ~/desktop-baseline/bench/*.txt
DEBUG=1 tools/desktop/bench.sh ~/desktop-baseline hl-s1 oa_dm1 1 4 1 0
tools/desktop/summarize.py --hunklog ~/desktop-baseline/bench/home-hl-s1/baseoa/console.log
```

Pinned revisions: spearmint `c48bdf60` (2026-05-25), mint-arena `482d5283`
(2026-04-07), spearmint-patch-data head of 2026-09-06. Build flags:
`BUILD_SERVER=0 BUILD_RENDERER_OPENGL2=0 USE_RENDERER_DLOPEN=0
USE_AUTOUPDATER=0`, everything else at Spearmint defaults (internal
FreeType, Ogg, Vorbis, Opus, libmad, jpeg, zlib; cURL and OpenAL through
dlopen). Both builds succeed with GCC 13.3 with no source changes.

Test content is OpenArena 0.8.8 (GPL) in `baseoa/`, with
`spearmint-patch-data/baseoa`, the fallback data and the Liberation fonts,
and the mint-arena QVMs placed in `baseoa/vm/`. `sv_pure 0`.

Output artifacts:

| File | Contents |
|---|---|
| `docs/baseline/summary.txt` | The table below, raw |
| `docs/baseline/hunk-breakdown-1seat.txt` | HUNK_DEBUG `hunklog` aggregated by label, 1 seat |
| `docs/baseline/hunk-breakdown-4seats-3bots.txt` | Same, 4 seats plus 3 bots |
| `docs/baseline/cgame-largest-symbols.txt` | `nm --size-sort` of the native cgame module |
| `docs/baseline/game-largest-symbols.txt` | Same for the game module |

## Results

Map `oa_dm1` (small) and `oa_rpg3dm2` (large). `def` = Spearmint default VM
heaps (`vm_gameHeapMegs 24`, `vm_cgameHeapMegs 2`); `small` = 4 and 1.
Times are milliseconds per frame, averaged over 300 frames after warm-up,
uncapped frame rate. Columns follow `com_speeds`: `all` total, `sv` server,
`cl` client (includes cgame VM and renderer front end submission), `gm` game
VM, `rf` renderer front end, `bk` renderer back end (llvmpipe).

| config | seats | bots | hunk MB | low perm | high perm | zone MB | all | sv | cl | gm | rf | bk |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| s1_def | 1 | 0 | 100.3 | 30.2 | 70.1 | 19.8 | 10.55 | 0.06 | 0.33 | 0.00 | 0.01 | 10.15 |
| s2_def | 2 | 0 | 101.6 | 30.9 | 70.8 | 19.8 | 19.36 | 0.20 | 0.72 | 0.01 | 0.12 | 18.31 |
| s4_def | 4 | 0 | 113.3 | 37.0 | 76.4 | 19.8 | 30.43 | 0.22 | 1.10 | 0.01 | 0.21 | 28.89 |
| s1_small | 1 | 0 | 84.3 | 30.2 | 54.1 | 19.8 | 11.16 | 0.07 | 0.34 | 0.00 | 0.05 | 10.70 |
| s2_small | 2 | 0 | 85.6 | 30.9 | 54.8 | 19.8 | 18.38 | 0.15 | 0.61 | 0.03 | 0.14 | 17.44 |
| s4_small | 4 | 0 | 97.3 | 37.0 | 60.4 | 19.8 | 30.83 | 0.24 | 1.46 | 0.02 | 0.26 | 28.85 |
| s1_bots_def | 1 | 3 | 100.3 | 30.2 | 70.1 | 19.8 | 12.68 | 0.29 | 0.61 | 0.03 | 0.01 | 11.72 |
| s1_bots_small | 1 | 3 | 84.3 | 30.2 | 54.1 | 19.8 | 10.98 | 0.17 | 0.31 | 0.01 | 0.01 | 10.47 |
| s4_bots_small | 4 | 3 | 97.3 | 37.0 | 60.4 | 19.8 | 22.95 | 0.24 | 1.56 | 0.02 | 0.17 | 20.96 |
| big_s1_small | 1 | 0 | 87.1 | 32.6 | 54.5 | 19.8 | 18.71 | 0.09 | 0.42 | 0.00 | 0.17 | 18.02 |
| big_s4_bots_small | 4 | 3 | 100.2 | 39.4 | 60.8 | 19.8 | 29.26 | 0.38 | 1.61 | 0.09 | 0.98 | 26.18 |

Hunk allocation is `com_hunkMegs 384` (Spearmint default; its floor is 56).

### Where the hunk goes (1 seat, small heaps, oa_dm1: 84.3 MB logged)

| MB | Allocation | Source |
|---:|---|---|
| 32.0 | Two VM data segments, 16 MB each | `vm.c:547` (`VM_LoadQVM`, power-of-two rounded) |
| 32.0 | Server `svs.snapshotEntities` = `sv_maxclients(8) * PACKET_BACKUP(32) * MAX_SNAPSHOT_ENTITIES(512) * 248 B` | `sv_init.c:306` via `common.c:2111` |
| 10.1 | `backEndData_t` (`MAX_DRAWSURFS 0x10000`, `MAX_REFENTITIES 4095`, skins) | `tr_init.c:1338` |
| 3.9 | VM `instructionPointers` (8 B per instruction on x86-64, 4 B on PPC32) | `vm.c:760` |
| 2.8 | MD3 model data | `tr_model.c:495` |
| 1.5 | Shader text | `tr_shader.c:4535` |
| 1.1 | World BSP surfaces, nodes, grid | `tr_bsp.c` |
| 0.4 | Collision map | `cm_load.c` |

The 19.8 MB of zone is dominated by the client `cl.parseEntities` darray
(`4 * 32 * 512 * 248 B` = 16.25 MB, `cl_cgame.c:2280`) plus 1 MB of entity
baselines.

### Going from 1 seat to 4 seats plus 3 bots

Only one line changes: MD3 model data 2.8 MB -> 15.8 MB (four different
default player models plus the bot model). Every other hunk allocation is
identical. The seat count itself is close to free in memory; distinct
player models are not.

### QVM segment sizes (from the QVM headers)

| Module | Instructions | Code | Data+lit | BSS | Data segment rounds to |
|---|---:|---:|---:|---:|---:|
| mint-cgame.qvm | 236,677 | 696 KB | 97 KB | 13,762 KB | 16 MB (with 1-2 MB heap) |
| mint-game.qvm | 278,050 | 812 KB | 55 KB | 4,830 KB | 8 MB with 3 MB heap, 32 MB with the 24 MB default |
| OpenArena 0.8.8 cgame.qvm (ioq3-style, for scale) | 136,054 | 393 KB | 39 KB | 3,821 KB | 4 MB |
| OpenArena 0.8.8 qagame.qvm | 227,181 | 666 KB | 66 KB | 1,554 KB | 2 MB |

Largest cgame BSS symbols (native build): `cg_polyBuffers` 6.5 MB
(`MAX_PB_BUFFERS 128` x `polyBuffer_t`), `cg_entities` 3.6 MB (4096 x
centity_t), `cgs` 0.8 MB, `tracemap` 0.75 MB, `cg` 0.55 MB, `cg_atmFx`
0.3 MB, `cg_localEntities` 0.2 MB. Largest game BSS: `g_entities` 3.5 MB,
`tracemap` 0.75 MB, `consolemessageheap` 0.3 MB, `botgoalstates` 0.2 MB.

## How to read this for the Wii

* CPU: `cl` grows from 0.33 to 1.46 ms and `rf` from 0.01 to 0.26 ms going
  from 1 to 4 seats on a fast x86 core. Broadway is roughly 30-60x slower on
  this kind of code (in-order, 729 MHz, no SIMD used by generic codegen, QVM
  interpreter or JIT rather than a native x86-64 JIT). A naive scaling puts
  the 4-seat client-side CPU cost at 45-90 ms per frame before the GX back
  end is counted. That is the number step 6 must measure honestly; it is
  why per-seat quality knobs and a 30 fps cap are in the plan from the start.
* Memory: the 84 MB hunk is not a Wii number, it is a list of knobs. See
  PORTING_PLAN section 4 for the budget that falls out of it.
