# Building and testing the Wii port

Status (2026-09-06): every Wii source file compiles for PowerPC and the
symbol audit finds nothing unresolved, but **no DOL has been linked or run
yet**: this sandbox has no devkitPPC and cannot reach devkitPro's package
host. The first link and the first boot happen on your machine. Expect a
short tail of link errors and boot bugs; the logging is there to find them.

## 1. Host prerequisites

* devkitPro with `devkitPPC`, `wii-dev` (libogc, libfat, libasnd). Build from
  the devkitPro shell (MSYS2 on Windows, or a shell with `DEVKITPRO` and
  `DEVKITPPC` exported on Linux/macOS). No portlibs are needed: zlib, jpeg
  and FreeType come from `engine/code`.
* A host C compiler and `make` for the QVM tools (`game/`), plus `zip`.
* Dolphin for iteration, and an RVL-001 Wii (the model with GameCube ports)
  running the Homebrew Channel for real testing.

## 2. Build the QVMs (host)

```
make -C game -j8 BUILD_GAME_SO=0 BUILD_GAME_QVM=1 BUILD_MISSIONPACK=0
```

Output: `game/build/release-<platform>/baseq3/vm/mint-cgame.qvm` and
`mint-game.qvm`. These carry the Wii trims (1024 entities, 8 poly buffers)
and must be paired with this tree's engine on desktop too.

## 3. Build the DOL

```
cd engine
make -f Makefile.wii            # release: engine/build-wii/boot.dol
make -f Makefile.wii debug      # boot.txt / diag.txt / crash.txt logging
```

Options: `WII_VM_NATIVE=0` (interpreter instead of the PowerPC JIT),
`WII_MAXFPS=60`, `WII_GX_PROFILE=1` (GP counters in `diag.txt`, debug
build), `WII_GAMEDIR=name` (data directory, default `newgame`),
`WII_BASEGAME=dir` (default `baseq3`).

Compile-only check without devkitPPC (what this sandbox ran):

```
sudo apt-get install gcc-powerpc-linux-gnu
git clone --depth 1 https://github.com/devkitPro/libogc ../libogc
git clone --depth 1 https://github.com/devkitPro/libfat ../libfat
# libogc/libfat need generated version headers:
printf '#define _LIBFAT_MAJOR_ 1\n#define _LIBFAT_MINOR_ 1\n#define _LIBFAT_PATCH_ 5\n#define _LIBFAT_STRING "libFAT"\n' > ../libfat/include/libfatversion.h
sed 's/@LIBOGC_MAJOR@/2/;s/@LIBOGC_MINOR@/9/;s/@LIBOGC_PATCH@/0/;s/@LIBOGC_VER@/2.9.0/' ../libogc/libversion.h.in > ../libogc/gc/ogc/libversion.h
make -f Makefile.wii check LIBOGC_SRC=../libogc LIBFAT_SRC=../libfat
tools/wii/check_qgl.sh          # every GL entry point the frontend calls is wired
```

## 4. Card layout

```
<dev>:/apps/newgame/boot.dol           (<dev> is sd: or usb:; SD is probed first)
<dev>:/apps/newgame/meta.xml           (optional HBC metadata)
<dev>:/newgame/baseq3/pak*.pk3         test content (Quake III Arena)
<dev>:/newgame/baseq3/vm/mint-cgame.qvm
<dev>:/newgame/baseq3/vm/mint-game.qvm
<dev>:/newgame/baseq3/wii.cfg          from data/wii.cfg (binds for four seats)
<dev>:/newgame/baseq3/mint-game.settings  from spearmint-patch-data/baseq3 (plus its scripts/ and models/)
<dev>:/newgame/baseq3/<fallback-data contents>   from spearmint-patch-data/fallback-data
<dev>:/newgame/fonts/*.ttf             from spearmint-patch-data/fonts
```

For OpenArena content use `baseoa/` with `spearmint-patch-data/baseoa` and
build with `WII_BASEGAME=baseoa`. Configs, `qkey` and logs are written next
to the pk3s (`fs_homepath` = `fs_basepath`).

The QVMs can also be zipped into a pk3 (`vm/mint-cgame.qvm`, `vm/mint-game.qvm`
inside `zz-newgame-vm.pk3`) once they stop changing every hour.

## 5. Running

* Pad 1 is seat 1 and starts playing when a map loads.
* Pads 2-4: press START on an unused pad to drop in (`IN_CheckDropIn`).
  Console equivalents: `2dropin`, `3dropin`, `4dropin`, `Ndropout`.
* Four seats from the start: `set cl_localPlayers 15; map q3dm1` in the
  console (the value is a bitmask, reset to 1 after each connect).
* Hold LEFT on pad 1 at boot for 240p NTSC, RIGHT for 264p PAL; default is
  the console's preferred 480i/576i, which four quadrants need.
* Press the POWER button to power off; RESET returns to the Homebrew Channel.

## 6. Dolphin

Dolphin boots the DOL directly (File > Open). Use a virtual SD image with
the layout above, enable "Insert SD Card", and map four GameCube
controllers. Dolphin does not reproduce memory exhaustion, cache
coherency or IOS timing, so treat it as a fast-iteration tool only.

## 7. Debugging on hardware

Build with `make -f Makefile.wii debug`. On the card after a run:

* `<dev>:/newgame/boot.txt`: boot timeline; the last line names the step
  that did not return.
* `<dev>:/newgame/diag.txt`: engine console output (`com_logfile 2`),
  `Sys_Error` text, JIT `mmap` slot log, GX backend warnings, and the GP
  profiler windows with `WII_GX_PROFILE=1`.
* `<dev>:/newgame/crash.txt`: written at boot and overwritten by
  `Sys_Error`; if it still says "main() started", the console died before
  any error path ran (usually a GX FIFO hang or a stack overflow).
* A `Sys_Error` also switches back to the text console and waits for START.

In-game: `meminfo` (hunk high-water marks), `hunklog` (debug build),
`com_speeds 1` (frame breakdown to `diag.txt`), `r_showImages 1`,
`r_speeds 1`.

## 8. What to expect at first boot, in order

1. Link errors from newlib/libogc API drift (the sandbox could not link).
2. `boot.txt` stopping before "Com_Init done": filesystem paths or the MEM2
   bump. `Wii_MEM2_Init` takes 40 MB off the top of Arena2; if libogc on
   your toolchain reserves more for IOS, lower `WII_MEM2_BUMP_MAX`.
3. A black screen after "Com_Init done": GX. `r_clear 1` paints the
   viewport magenta if the copy path works at all.
4. Textures wrong or flickering: cache flush on a new buffer path. Every GX
   array must come from `GXBE_DrawTess`'s ring or be `DCFlushRange`d.
5. Pad 2-4 not joining: check that `2dropin` from the console works first;
   then that `IN_Frame` sees the channel (`PAD_ScanPads` connected mask).

## 9. Features cut for the first playable, and the recommendation on each

| Cut | Saves | Recommendation |
|---|---|---|
| Wiimote, Nunchuk, Classic Controller, Wii U GamePad, USB HID pads, USB keyboard | Bluetooth stack, USB stack, about 1.5k lines of input code, a class of hangs | Keep cut. The brief fixes GameCube-only. |
| Networking (sockets, LAN browser, downloads, master servers) | libogc network init, `net_ip.c`, cURL | Keep cut for the first playable; the donor's shim exists if LAN play is ever wanted. |
| OpenAL, Ogg Vorbis, Opus, MP3, VoIP, Mumble | about 1 MB of code, decode CPU | Keep cut. Test content is WAV. Music later means Vorbis or pre-decoded WAV; decide when there is music. |
| OpenGL2 renderer, AVI capture, autoupdater, non-Q3 BSP loaders | code size | Keep cut. |
| Stencil shadows (`cg_shadows 2`), `r_measureOverdraw`, GL fog (`r_useGlFog`), wireframe | GX has no stencil and no user clip planes | Keep cut. Blob shadows (`cg_shadows 1`) are a CGame choice for later. |
| Model LOD levels 1 and 2 | 30-50% of player model hunk | Keep cut; `r_lodbias` is meaningless on this backend anyway. |
| Four distinct default player models | 13 MB measured on desktop | Ship one or two models; set `2model`..`4model` in `mint-game.settings`. |
| 65536 draw surfaces, 4096 entities, 128 poly buffers, 32-deep snapshot history | about 45 MB of the 84 MB desktop hunk | Done. Reverse only with a measured reason. |
| Bots | game VM heap (AAS route cache) | Keep, but with `max_routingcache 1024` and two bots until hardware numbers exist. |
| FreeType runtime fonts | about 500 KB code plus glyph atlases | Keep for now; pre-render to `fontImage_*` files if `meminfo` shows the atlases. |
| 60 fps cap | nothing to save; it is the target | Start at 30. Four `R_RenderView` passes on a 729 MHz core will not hold 60. |
| Music, cinematics, demos, screenshots | already compiled in; cost nothing until used | No action. |

Features deliberately kept: drop-in/drop-out, per-seat binds and analog
settings, bots, the in-game console (via the CGame console; a USB keyboard
is not needed because `wii.cfg` and `+set` cover configuration).
