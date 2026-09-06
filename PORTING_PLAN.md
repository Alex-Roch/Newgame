# Porting plan: 4-seat splitscreen FPS on retail Wii (GameCube pads only)

Status: end of step 1 (recon) plus step 2 (desktop baseline). No Wii code
has been written. Report before step 3, as requested.

Companion documents: `SPLITSCREEN_NOTES.md` (how seats work),
`docs/DESKTOP_BASELINE.md` (numbers), `LICENSING_NOTE.md`.

## 1. What the trees actually say (corrections to the brief)

Every item below was checked against source, not READMEs.

1. **The donor is vendored from current ioq3 main, not an old revision.**
   Blob fingerprinting puts 178 of its 251 vendored engine files byte-identical
   to ioq3 `58839361` (2026-07-16), the current head. Its engine-side patch
   set is about 2,500 changed lines across 32 files, of which `msg.c` (847
   lines) and most of `files.c` are the CLASSIC/Dreamcast protocol-43
   flavour we will not carry. The renderer-side patches are about 1,100 lines.
2. **Spearmint tracks ioq3 closely.** Merge base with ioq3 main is
   `d8b1769d` (2025-06-16). Spearmint is 1,713 commits ahead of that base;
   ioq3 is 263 ahead. The two frontends share ancestry only a year old.
3. **Renderer drift is large but mostly irrelevant to the port.** Spearmint's
   `renderergl1` differs from ioq3's by about 14,900 changed lines (`tr_shader.c`
   +2,138, `tr_model.c` +1,426, `tr_bsp.c` +859, plus new files
   `tr_animation_mdm.c`, `tr_animation_mds.c`, `tr_cmesh.c`, `tr_model_tan.c`).
   But the donor's GX backend is **not** a rewrite of the frontend. In its
   `WII_NATIVE_GX` build every `qgl*` pointer is a no-op except seven routed
   ones (`qglEnable/Disable`, `qglPolygonOffset`, `qglVertexPointer`,
   `qglColorPointer`, `qglTexCoordPointer`, `qglDepthRange`), and the real work
   happens through about 30 `GXBE_*` functions called from `#if WII_NATIVE_GX`
   blocks at the frontend's choke points: `GL_Bind`, `GL_SelectTexture`,
   `GL_Cull`, `GL_TexEnv`, `GL_State`, `SetViewportAndScissor`,
   `RB_BeginDrawingView`, `RB_RenderDrawSurfList` (modelview loads),
   `RB_SetGL2D`, `RE_StretchRaw`, `RE_UploadCinematic`, `R_DrawElements`,
   `Upload32`, `R_CreateImage`, `R_DeleteTextures`, sky box drawing, flares
   depth peek, `RB_ShowImages`, and screenshot readback. Spearmint has every
   one of those functions under the same names. The frontend delta that
   matters is the set of GL entry points Spearmint uses that ioq3 does not:
   `qglFogf/fv/i`, `qglHint`, `qglColor3fv`, `qglPointSize`,
   `qglCompressedTexImage2DARB`, `qglGetTexLevelParameteriv`. Fog maps to
   `GX_SetFog`; the rest are debug or optional.
4. **The PPC JIT question has a good answer.** Spearmint still carries
   `vm_powerpc.c` + `vm_powerpc_asm.c` (the pre-merge ioq3 split; ioq3 merged
   the assembler into one file in `2c91b388`). Spearmint's copy differs from
   that pre-merge ioq3 file by 22 lines. The donor's JIT changes (GEKKO guard
   because devkitPPC does not define `__powerpc__`, `ogc/cache.h` for
   `DCFlushRange`/`ICInvalidateRange`, a 512 KB chunked arena replacing
   per-node malloc) are 212 lines against the merged file and re-apply to the
   split file mechanically. The JIT mmaps its code buffer separately from
   `vm->dataBase` (`vm_powerpc.c:1824`) and Spearmint's heap lives *inside*
   `dataBase` (`vm.c:551-557`, zone-tagged via `Z_VM_InitHeap`), so the
   donor's mmap reuse slots and Spearmint's heap model do not touch each
   other. They coexist without design work.
5. **One donor claim is stale in its own docs.** The donor's `AGENTS.MD`
   says the non-power-of-two `dataAlloc` change is "NOT in the tree". It is
   (`vm.c` `#if defined(GEKKO) ... wiiDataAlloc`, plus `programStack =
   dataAlloc - 4`). It cannot be carried to Spearmint as is, because
   Spearmint deliberately uses the rounding slack as the VM heap. The
   equivalent saving on Spearmint is to size `vm_*HeapMegs` so BSS plus heap
   lands just under a power of two.
6. **Spearmint's memory defaults are far outside the Wii.** `com_hunkMegs`
   default 384 with a hard floor of 56 (`common.c:52-53`), `com_zoneMegs`
   64 (floor 64), `vm_gameHeapMegs` 24, `vm_cgameHeapMegs` 2,
   `MAX_SNAPSHOT_ENTITIES` 512 (ioq3: 256), `PACKET_BACKUP` 32,
   `MAX_DRAWSURFS` 65536, `REFENTITYNUM_BITS` 12. All are compile-time or
   cvar knobs; none are architectural. Measured consequences are in section 4.
7. **mint-arena's cgame QVM has 13.8 MB of BSS**, versus 3.8 MB for an
   ioq3-era cgame. 6.5 MB of it is `cg_polyBuffers[128]` and 3.6 MB is
   `cg_entities[4096]` (`REFENTITYNUM_BITS`/`GENTITYNUM_BITS` 12). The game
   QVM has 4.8 MB (3.5 MB `g_entities`). These are constants.
8. **The AAS bot library is compiled into the game QVM** (`mint-arena/Makefile`
   lines 1423-1439) and allocates from `trap_HeapMalloc`. That is why the
   default game heap is 24 MB. Bots are therefore a VM-heap budget item, not
   an engine one. `max_routingcache` (default 4096 KB) and `max_aaslinks`
   are the levers.
9. **The renderer is completely seat-blind** (zero `MAX_SPLITVIEW`
   references in `renderergl1`). CGame computes viewport rectangles and
   calls `RenderScene` once per seat. This is the best possible shape for GX:
   one `R_RenderView` per seat with its own viewport and scissor, no EFB
   copies between seats.
10. **The brief's reasoning for choosing Spearmint holds.** Confirmed in
    source: console, notify, chat input, menus, usercmd creation and the AI
    are in the VMs; the engine carries per-seat cmds, snapshots, area masks,
    joystick events and sound listeners. Reimplementing this on stock ioq3
    would be reimplementing Spearmint. No alternative is recommended.
11. **Environment limitation of this session:** devkitPro's package host and
    GitHub release assets are blocked by the sandbox proxy, so the PPC
    toolchain could not be installed here. All Wii compilation must happen
    on a machine with devkitPPC; nothing in this report was verified by a
    PPC build. This is stated in the risk register.

## 2. Architecture decision

Take Spearmint as the engine. Replace its `code/sys` + `code/sdl` layer with
a new, small libogc-native layer written for this project, using the donor
as a worked example rather than as code to port line by line. Port the
donor's GX backend (`code/renderer/tr_gx*.c`, `qgl_wii.c`) and re-apply its
choke-point patches onto Spearmint's `renderergl1`. Keep QVMs and the PPC
JIT. Build one flavour.

Why not port the donor's sys layer as is: it is 2,088 lines but a third of
it is Wiimote/USB-HID/mod-selector/CLASSIC machinery that this project
deletes, and Spearmint's `Sys_*` surface differs from ioq3's in a dozen
places (`Sys_Dialog`, `Sys_ParseProtocolUri`, `Sys_StatFile`, PID files,
`Sys_GetProcessorFeatures`, `CL_MaxSplitView` import, `Sys_SetEnv`,
per-seat joystick cvars). Writing 800-1,200 fresh lines against Spearmint's
actual `sys_local.h` is less work than editing 2,000 lines of someone
else's against the wrong header.

## 3. Subsystem work breakdown

Effort is in focused engineering days, assuming a devkitPPC host and a
console with four ports. Ranges are honest; the right-hand number includes
the debugging tail on hardware.

### 3.1 Build system and toolchain (2-3 days)

* New top-level `Makefile.wii` modelled on the donor's: `-include
  wii_platform.h` before every TU, `-DGEKKO -DWII`, big-endian defines,
  `-msdata=none -G 0`, `rvl.ld`, `elf2dol`.
* Object list from Spearmint's `Q3OBJ` minus SDL/curl/OpenAL/codec/mumble/
  autoupdater/avi objects, plus the port layer.
* Force `NO_VM_COMPILED` off, `HAVE_VM_COMPILED` on, build `vm_powerpc.c` +
  `vm_powerpc_asm.c`.
* Internal zlib and jpeg from Spearmint's tree (`USE_INTERNAL_*`), FreeType
  decision per section 5.
* Host side: mint-arena's own `q3lcc`/`q3asm` build the QVMs; QVMs are
  endian-neutral files, byte-swapped at load (`VM_LoadQVM` uses
  `LittleLong` on data, JIT and interpreter both handle it).
* Deliverable: `boot.dol` that links. Expect two days of `#ifdef` and
  header work: Spearmint compiles with `-Werror=implicit-function-declaration`
  style flags and uses `Sys_*` calls the donor never implemented.

### 3.2 Platform layer, libogc-native (5-8 days)

New files, roughly:

| File | Content | Lines (est.) |
|---|---|---:|
| `wii_main.c` | `main()`: console init, MEM2 bump reserve, FAT mount, stack, power/reset callbacks, cmdline, `Com_Init`, frame loop | 300 |
| `wii_sys.c` | `Sys_Milliseconds` (with base subtraction), `Sys_Error`/`Sys_Quit`, file and dir ops, `Sys_ListFiles`, `Sys_Mkdir`, `Sys_StatFile`, stubs for dialogs, PID, clipboard, dll loading returning NULL, `Sys_RandomBytes` | 500 |
| `wii_mem.c` | MEM2 bump allocator, `calloc` wrap for large blocks, `mmap`/`munmap` slot table for JIT code, malloc lock wrappers | 200 |
| `wii_glimp.c` | VI mode select, XFBs, GX FIFO in MEM1, EFB format, copy filter, `GLimp_Init` filling `glConfig`, end-of-frame copy | 250 |
| `wii_input.c` | `PAD_ScanPads` once per frame, four channels to `SE_JOYSTICK_AXIS/BUTTON + chan`, deadzone, WaveBird works for free, optional USB keyboard behind `WII_DEBUG_KEYBOARD` | 250 |
| `wii_snd.c` | `SNDDMA_*` on ASND/AESND with a ring, 22 kHz stereo | 150 |
| `wii_net.c` | Either nothing (see 3.6) or the donor's loopback-only shim | 0-150 |
| `wii_log.c` | `boot.txt`/`diag.txt`/`crash.txt` writers, `wii_diag()`, persistent handle, fsync only on error paths | 100 |

Reuse from the donor verbatim where the interface is identical:
`Sys_Milliseconds` base subtraction, MEM2 init, `mmap` slots, the
`__wrap_calloc` trick, malloc lock, ASND ring, the video-mode selection
table (default / 240p NTSC / 264p PAL), the "exit(0) returns to HBC" rule.
Delete: WPAD, IR, Classic Controller, libwiidrc, USB HID pads, controller
priority chain, per-controller bind files, mod selector, CLASSIC zpack
extraction, cdkey wrapper (`__wrap_VM_Call` for UI CD key; Spearmint has no
UI VM and no CD key).

Spearmint-specific extras the donor never needed:

* `CL_MaxSplitView` and `CL_GlconfigChanged` renderer imports (already in
  `common.c`'s `CL_InitRef`-equivalent; nothing to write).
* `Sys_LoadGameDll` returns NULL so `FS_FindVM` falls through to QVMs.
* `con_passive.c` as the console backend (no TTY).
* `Sys_Dialog` prints to `diag.txt`.

### 3.3 Memory model (3-5 days, then continuous)

Compile-time and cvar changes, all in one `wii_platform.h` plus a few
`#ifdef GEKKO` lines:

* `MIN_COMHUNKMEGS` 56 -> 8, `DEF_COMHUNKMEGS` -> computed from the MEM2
  bump (donor: bump minus 1 MB), `DEF_COMZONEMEGS` 64 -> 6 and its floor.
* `PACKET_BACKUP` 32 -> 16 (donor did this; `MAX_RELIABLE_COMMANDS` stays 64).
* `MAX_SNAPSHOT_ENTITIES` 512 -> 256.
* `sv_maxclients` 8. Server snapshot entities then cost
  8 x 16 x 256 x 248 B = 8.1 MB; client parse entities 4 x 16 x 256 x 248 B =
  4.1 MB. A local-only build can additionally use the `Com_GameIsSinglePlayer`
  branch (`x 4` instead of `x PACKET_BACKUP`), giving 2.0 MB and 1.0 MB. We
  will do the latter since the loopback link never drops packets.
* `MAX_DRAWSURFS` 0x10000 -> 0x4000 and `REFENTITYNUM_BITS` 12 -> 10 bring
  `backEndData_t` from 10.1 MB to about 2 MB. Both are drawsurf sort-key bit
  fields; check `DRAWSURF_BITS` packing in `tr_local.h` when changing them.
* VM heaps: `vm_cgameHeapMegs` and `vm_gameHeapMegs` chosen so BSS + heap
  sits just under a power of two. With mint-arena as is: cgame 13.8 MB BSS +
  2 MB heap = 16 MB; game 4.8 MB + 3 MB = 8 MB. With the cgame BSS cuts in 3.7
  (polybuffers 128 -> 8, `cg_entities` 4096 -> 1024): cgame about 5 MB + 2
  MB heap = 8 MB. Game with `g_entities` 4096 -> 1024: 2.2 MB + 1.8 MB = 4 MB.
* `instructionPointers` on PPC32 is 4 B per instruction: about 2 MB for both
  VMs. Acceptable.
* JIT code buffers off-hunk in the MEM2 bump (donor's slot table). Expect
  about 3-4 MB for both VMs given 515k instructions.

Resulting first-cut Wii hunk budget for a `oa_dm1`-class map, one shared
player model, no bots:

| Item | MB |
|---|---:|
| cgame VM data (after BSS cuts) | 8 |
| game VM data (after BSS cuts) | 4 |
| instruction pointers, jump tables | 2 |
| server + client snapshot entity arrays (local-only sizing) | 3 |
| `backEndData_t` | 2 |
| world BSP, collision, shaders text, images metadata | 4-6 |
| models (one player model, weapons, items) | 3-5 |
| sound (2 MB pool, ADPCM 4:1, on sbrk not hunk) | 0 |
| **Total hunk** | **26-30** |

That fits the donor's 32 MB bump with 2-6 MB of headroom for a second
player model and a few bots' AAS cache. It does *not* fit four distinct
player models (13 MB measured) or Spearmint's default limits. Content scope
therefore has to be decided against this table, which is exactly step 7.

### 3.4 Renderer (6-10 days)

* Copy `tr_gx.c`, `tr_gx.h`, `tr_gx_texture.c`, `qgl_wii.c`, `wii_gl_stubs.c`
  into `code/renderergx/` and compile them against Spearmint's `tr_local.h`.
  Expected friction: `tess` layout (Spearmint's `shaderCommands_t` has more
  arrays; the backend only uses `xyz`, `svars.colors`, `svars.texcoords`,
  `numVertexes`, `numIndexes`), `glState` fields, `image_t` (Spearmint has
  `RegisterShaderEx` and `LIGHTMAP_*` changes but `texnum`, `uploadWidth`,
  `wrapClampMode` exist), `viewParms_t` (same fields plus `isMirror`).
* Re-apply the choke-point patches (about 1,100 lines, listed in section 1
  item 3) onto Spearmint's files. Each is a `#if defined(WII_NATIVE_GX)`
  block next to existing code. `tr_backend.c` is the largest at 241 lines;
  Spearmint's `tr_backend.c` is 1,431 lines versus ioq3's 1,151 but the
  functions are the same. Budget one day for the diff, one for the
  parts that moved (`RB_RenderDrawSurfList` gained sort-level handling,
  `RB_BeginDrawingView` gained sky/no-sky flags).
* Implement the eight new entry points: fog via `GX_SetFog` (Spearmint uses
  GL fog for its global fog and view fog: `tr_main.c:146-174`), `PointSize`
  and `Color3fv` as no-ops, compressed textures disabled
  (`glConfig.textureCompression = TC_NONE`), `GetTexLevelParameteriv`
  unreachable once compression is off.
* Texture path: keep the donor's RGB565/RGB5A3 swizzle and in-place mip
  generation. Spearmint's `Upload32` signature differs slightly (it passes
  `imgType`/`imgFlags`); the donor's "smuggle texnum through file statics"
  trick should be replaced by adding two parameters, which is cleaner and the
  frontend is ours to edit.
* Fonts: `RE_RegisterFont` in Spearmint takes `borderWidth`, `forceAutoHint`
  and rasterises TrueType at runtime through FreeType. See section 5.
* Per-seat: nothing new. Verify the 2D clip region (`RE_SetClipRegion`)
  reaches `GX_SetScissor` through `RB_SetGL2D`; today the donor's 2D path
  sets the full-screen scissor once, which would let one seat's HUD bleed
  into another's. That is the one seat-specific renderer bug to expect.
* Known GX gaps inherited from the donor and acceptable: no stencil (no
  stencil shadows, no `r_measureOverdraw`), no user clip planes (portals use
  oblique near-plane projection instead), no wireframe, no DST_ALPHA blends
  (EFB is RGB8_Z24), max texture 1024.
* Performance note for step 6: `GXBE_DrawTess` streams every index through
  the CPU write-gather pipe (`GX_Position1x16` per index, 3-4 words per
  index) and copies the whole `tess` into a staging ring per draw. For four
  seats this is the first thing to profile. Two cheap wins are available
  without touching the frontend: 16-bit texcoords/positions where precision
  allows, and skipping the staging copy for surfaces whose vertex data lives
  in the BSP (world surfaces, which are immutable). A larger win, display
  lists for static world surfaces, needs the frontend to stop rewriting
  `tess` for those, which is a real project and should wait for numbers.

### 3.5 VM and JIT (2-4 days)

* Apply the donor's `vm_powerpc.c` changes to Spearmint's split
  `vm_powerpc.c`/`vm_powerpc_asm.c`: GEKKO include guard, arena allocator,
  `PPC_ArenaReset`, `DCFlushRange` + `ICInvalidateRange` before `mprotect`
  (which is a no-op stub on Wii).
* `vm.c`: keep Spearmint's heap model; add only the diagnostic prints and
  the `Hunk_MemoryRemaining` logging the donor added.
* Verify `VM_BlockCopy`, `VM_ArgPtr` masking and `VM_ExplicitArgPtr` behave
  with the JIT: `CG_CREATE_USER_CMD` returns a VM pointer that the engine
  reads back (`cl_input.c:225`), which is new relative to ioq3. The
  interpreter path already handles it; the JIT returns an `intptr_t` the
  same way, but this is the first thing to test in step 3.
* Fallback if the JIT misbehaves on a mint-arena opcode mix the donor never
  ran: `WII_VM_NATIVE=0` interpreter, which needs about 4 B per instruction
  more (`vm_interpreted.c` code copy). That is 2 MB, affordable after the
  memory work above. Native game modules are not needed; see section 6.

### 3.6 Networking (1 day to decide, 1-2 days to do)

Spearmint's local server still speaks to the local client through
`NET_SendPacket` on the loopback queue: `Sys_SendPacket` is only called for
real addresses, and `NA_LOOPBACK` traffic goes through `NET_SendLoopPacket`
in `net_chan.c` without touching sockets. `NET_Init` in `net_ip.c` opens
sockets, resolves interfaces and reads `net_enabled`. Plan: compile
`net_ip.c` with `net_enabled 0` semantics forced, stub `Sys_IsLANAddress`,
`Sys_StringToAdr`, `Sys_SendPacket`, `Sys_ShowIP`, and do **not** call
libogc's `net_init` at all. Confirm on the headless boot (step 3) that
`map` connects over loopback with no socket. Keep the donor's `wii_net.h`
shim in the tree, unused, so LAN play can be reconsidered later.

### 3.7 Game code (mint-arena) changes (3-5 days now, more later)

Required for the Wii budget:

* `MAX_PB_BUFFERS` 128 -> 8 (`cg_polybus.c:33`), only atmospheric effects and
  Team Arena UI use poly buffers.
* `GENTITYNUM_BITS` 12 -> 10 and `REFENTITYNUM_BITS` 12 -> 10 in both engine
  and game headers (they must match the engine; both are shared constants).
  1024 entities is Quake 3's own limit and enough for a four-player arena.
* Bots: run with `max_routingcache 1024` and measure `trap_HeapAvailable`
  after AAS load on the largest intended map; size `vm_gameHeapMegs` from
  that.
* Default GameCube layout as a `joyremap`/bind set applied on first run for
  all four seats (`in_joystickUseAnalog 1` per seat, analog look speeds).
* `cvarDefault` in a project `mint-game.settings`: `2model`..`4model` set to
  the one or two models the content budget allows.

Not required now but expected (the "seat bug tail" the brief warns about):
per-seat notify overlap at quarter resolution, `cg_splitviewTextScale` at
320x240 quadrants, scoreboard in the fourth quadrant, team chat box, HUD
elements sized for 640x480 that do not survive 320x240, `cg_viewsize`. This
is CGame-only work and can be iterated in Dolphin.

### 3.8 Instrumentation (1-2 days, first)

Port the donor's `boot.txt` timeline, `diag.txt` + `Sys_Error` trace,
`crash.txt` early checkpoint, and the `WII_GX_PROFILE` GP counter windows
(`xf_wait_out`, `fifo_req`, triangles, vertices). Add `meminfo` and
`hunklog` output to `diag.txt` on map load. Add a `com_speeds` to
`diag.txt` toggle. This lands before any renderer code.

### 3.9 Testing sequence (as in the brief, one change)

1. Recon: done.
2. Desktop baseline: done (`docs/DESKTOP_BASELINE.md`).
3. Headless PPC boot: `boot.dol` with the `DEDICATED`-style headless
   renderer (`sv_ref.c` `GetRefAPI(..., headless=qtrue)` exists in Spearmint
   for exactly this), mounting FAT, loading pk3s, creating both VMs with the
   JIT, `map` over loopback, writing `boot.txt`. This proves toolchain,
   endianness, memory model, JIT, loopback, in that order. Add a headless
   4-seat connect (`cl_localPlayers 15`) to prove the server-side seat path
   before any pixels exist.
4. Renderer, one seat, parity with the donor on the same map and data.
5. GameCube input, four channels.
6. Two seats on hardware and profile.
7. Four seats, then content scope.

Change from the brief: step 3 should already run the *client* engine in
headless mode, not just the server, because `CL_CreateCmd` -> VM pointer
return and the per-seat snapshot parse are the parts most likely to break
on big-endian with the JIT, and they need no renderer.

### 3.10 Effort summary

| Area | Days |
|---|---:|
| Build system, toolchain | 2-3 |
| Platform layer | 5-8 |
| Memory model and limits | 3-5 |
| Renderer port | 6-10 |
| VM/JIT | 2-4 |
| Networking removal | 2-3 |
| Game code (budget cuts, defaults, pad layout) | 3-5 |
| Instrumentation | 1-2 |
| Hardware bring-up, 2 then 4 seats, profiling, first round of seat bugs | 8-12 |
| **Total to a 4-seat playable on test content** | **32-52** |

Original content is outside this estimate.

## 4. Risk register, ordered

1. **CPU time per frame with four seats (high).** Desktop shows client-side
   cost scaling 4.4x from one to four seats; the donor reports the backend as
   the bottleneck at one seat on Wii. Four `R_RenderView` passes plus four
   cgame frames on a 729 MHz in-order core may not reach 30 fps on real maps.
   Mitigations, in order: 30 fps cap from day one; per-seat `r_lodbias`,
   `cg_shadows 0`, `r_dynamiclight 0`, `cg_marks 0`, `r_flares 0`; smaller
   maps; drop the staging copy for world surfaces; paired-singles for
   `RB_CalcDiffuseColor`/`RB_DeformTessGeometry` only if profiling names them.
   Decision point: after step 6 numbers, not before.
2. **Memory (high, but bounded).** Every large line item is a known constant
   (section 3.3). The unknown is content: four distinct player models cost
   13 MB on desktop. Mitigation: one or two shared models, `_1`/`_2` LOD
   skip (donor patch), 16-bit textures, `r_picmip 2`. The Team Arena failure
   in the donor is the cautionary case.
3. **Untested combination: mint-arena on the PPC JIT (medium).** The donor
   ran Q3A/OA QVMs; mint-arena's are 2x the instruction count and use
   `trap_HeapMalloc`, `VM_ExplicitArgPtr` returns, and byte-swap helpers.
   Mitigation: interpreter fallback is one build flag; step 3 tests this
   first.
4. **GX cache-coherency corruption (medium).** Any new buffer handed to GX
   without `DCFlushRange` and 32-byte alignment shows up as intermittent
   garbage. Mitigation: keep all vertex traffic inside `GXBE_DrawTess` and
   the immediate-mode helpers; do not add new GX array users; keep the ring
   fence design.
5. **Frontend patch drift (medium-low).** Spearmint's `tr_backend.c`,
   `tr_shade.c`, `tr_image.c` have moved; a missed choke point yields a
   silently missing effect rather than a crash. Mitigation: A/B parity
   against the donor on identical data in step 4 with `r_showImages`,
   `r_lightmap`, `r_showtris`-style checks that do work on GX.
6. **HUD at 320x240 quadrants (medium, CGame-only).** Spearmint's own
   changelog lists a tail of splitscreen HUD fixes at desktop resolutions;
   at quarter-480i the text scale, notify area and scoreboard will need
   rework. Iterable in Dolphin.
7. **Toolchain environment (process risk).** devkitPPC could not be installed
   in this sandbox. All PPC builds and the step 3 boot must run on the
   user's devkitPro host or a session with access to `pkg.devkitpro.org`.
8. **Licensing at release (low now, real later).** See `LICENSING_NOTE.md`.
9. **Console model (low, but check first).** GameCube ports exist only on
   RVL-001. Confirm before step 5.

## 5. Dependency inventory and proposed cuts

| Dependency | Spearmint default | Cost | Proposal |
|---|---|---|---|
| SDL2 (video, input, audio) | on | whole sys layer | Replace with libogc-native (section 2) |
| cURL (dlopen) | on | code size, `cl_curl.c` | Off (`USE_CURL=0`); no downloads |
| OpenAL (dlopen) | on | `snd_openal.c`, `qal.c` | Off; software mixer only |
| Ogg Vorbis (internal libogg + libvorbis) | on | ~300 KB code, decode CPU | Off for first playable; all test content is WAV; revisit for music streaming |
| Opus + opusfile | on | ~400 KB code | Off (VoIP and Opus music both unnecessary) |
| libmad MP3 | on | ~150 KB | Off |
| VoIP, Mumble link | on | small | Off (`USE_VOIP=0`, `USE_MUMBLE=0`) |
| FreeType 2.9 (internal) | on | ~500 KB code, glyph atlases per font size at runtime (256x256 RGBA per size), fonts on SD (950 KB TTF) | Keep for the first playable, since mint-arena's UI and HUD call `trap_R_RegisterFont` and Spearmint has no bitmap-font fallback for TrueType names. Measure atlas count on hardware; if it matters, pre-render to `fontImage_*.tga` + `.dat` on the desktop build and ship those (`tr_font.c` already loads the pre-rendered format when present). |
| libjpeg 8c (internal) | on | needed for `.jpg` textures in any Q3 data | Keep; the donor links portlibs libjpeg, we use Spearmint's internal copy |
| zlib (internal) | on | pk3 | Keep |
| Autoupdater | off | none | Stays off |
| AVI recording (`cl_avi.c`) | built | small | Compile out |
| Renderer OpenGL2 | built | large | Not built (`BUILD_RENDERER_OPENGL2=0`) |
| Botlib residue in engine (`l_precomp`, `l_script`, `l_memory`, `l_struct`) | built | small | Keep; the server uses `l_precomp` for `.aas`-independent parsing |
| BSP loaders for other games (`bsp_fakk.c`, `bsp_sof2.c`, `bsp_ef2.c`, `bsp_mohaa.c`, `bsp_q3ihv.c`, `bsp_q3test*.c`) | built | ~100 KB code | Compile out; keep `bsp_q3.c` |

## 6. Decisions requested, with recommendations

1. **SDL2 versus libogc-native sys layer.** Recommend **libogc-native, written
   fresh against Spearmint's `sys_local.h`**, with the donor as reference.
   There is no SDL2 for Wii worth using, and the donor's layer targets a
   different `Sys_*` surface and carries hardware we are deleting.
2. **Port the donor GX backend versus write a fresh one.** Recommend
   **port the donor backend** (`tr_gx*.c`, `qgl_wii.c`) and re-apply its
   1,100 lines of choke-point patches. The interface is 30 functions and the
   patches are localised; a fresh backend would end up in the same shape
   and lose a year of the donor's hardware debugging (ring fences, oblique
   portal projection, TV-safe insets, mip generation, texture swizzle).
   Reserve a rewrite of `GXBE_DrawTess` only, after profiling.
3. **QVM versus native game modules.** Recommend **QVM with the PPC JIT**,
   interpreter as the build-flag fallback. The JIT is present in Spearmint,
   the donor's fixes apply mechanically, and the heap model coexists. Native
   modules would remove the sandbox and byte-swap discipline, but they also
   remove the host-side LCC build that lets game code iterate in Dolphin
   without relinking the DOL, and they make `VM_ExplicitArgPtr`-style code
   behave differently from desktop. Revisit only if step 3 shows the JIT
   cannot run mint-arena.
4. **Keep mint-arena versus fresh game VMs.** Recommend **keep mint-arena,
   trimmed**, for the first playable: it is the only game code that
   exercises Spearmint's full splitscreen API (dropin, per-seat HUD, console,
   menus, bots), and "original game" for this project should be built by
   subtraction and replacement inside it, not from a blank `vmMain`. The
   trims are constants (section 3.7). A fresh VM is a later content decision
   with a stable engine underneath it.
5. **Target seat count for the first playable.** Recommend **two seats**
   on hardware as the first playable milestone (step 6), with four seats as
   the step 7 gate. Two seats already exercise every per-seat path; four
   seats are a performance and content question that the two-seat numbers
   answer. Building the four-seat layout in Dolphin in parallel costs
   nothing.
6. **Additional decision surfaced by recon: video mode.** Recommend
   **480i default with the donor's 240p/264p boot prompt kept**. At 240p
   each quadrant is 160x120, which is unplayable; at 480i quadrants are
   320x240. Field rendering off, EFB RGB8_Z24, copy filter on.
7. **Additional: networking.** Recommend **remove the IP stack from the
   first playable** (section 3.6) and keep the donor's shim in-tree unused.
8. **Additional: FreeType.** Recommend **keep for now, pre-render fonts
   later if the atlas memory shows up in `meminfo`.**

## 7. Where I disagree with the brief

* "Renderer: the size of this task is set by how far the frontend has
  drifted." Not quite. The drift is 15k lines, but the backend contract is
  30 functions and 1,100 lines of localised patches. The size of the task is
  set by the patch re-apply plus the eight new GL entry points, and it is
  6-10 days, not a rewrite.
* "Consider dropping networking. Confirm the loopback path survives without
  it." Confirmed in source: loopback never touches `Sys_SendPacket`. Plan
  is to drop it, not just consider it.
* "Spearmint replaced ioq3's VM memory model." It extended it: same
  power-of-two `dataMask` and hunk allocation, with the rounding slack
  turned into a tagged zone heap. This is why the donor's non-power-of-two
  trick cannot carry over, and why heap sizing is done by choosing
  `vm_*HeapMegs`, not by patching `vm.c`.
* The brief lists mint-arena's FreeType fonts, Ogg, Opus and MP3 as a RAM
  and CPU cost to inventory. They are, but the far larger costs are
  Spearmint's default limits (`MAX_SNAPSHOT_ENTITIES`, `PACKET_BACKUP`,
  `MAX_DRAWSURFS`, `REFENTITYNUM_BITS`, `vm_gameHeapMegs`) and mint-arena's
  cgame BSS. Those are 60 of the 84 MB measured, and all are constants.
* The brief's step 3 is server-only. Make it client-inclusive and headless;
  Spearmint already has the headless renderer mode to do it.
