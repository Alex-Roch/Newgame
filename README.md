# newgame: 4-player splitscreen FPS for retail Wii (GameCube pads only)

Private project. Engine base is Spearmint (ioquake3 fork with four-player
splitscreen), game code is mint-arena, and the Wii platform layer is
derived from Mayo1970's ioQuake3-wii with the author's permission.

Current state: the Wii port is written and compile-checked for PowerPC
(every object builds with a cross compiler against libogc headers, symbol
audit clean), but it has not been linked or booted: this was done in an
environment without devkitPPC. See `docs/BUILD_WII.md` for the first build
and boot on real tooling, and for the list of features cut.

| Document | Purpose |
|---|---|
| `PORTING_PLAN.md` | Findings, corrections to the original brief, work breakdown, estimates, risk register, decisions requested |
| `SPLITSCREEN_NOTES.md` | How Spearmint's splitscreen works, by file, function and cvar |
| `docs/DESKTOP_BASELINE.md` | Reproducible Linux build of Spearmint + mint-arena and the measured memory and frame-time baseline |
| `docs/baseline/` | Raw baseline output (meminfo, hunk breakdown, symbol sizes) |
| `LICENSING_NOTE.md` | Licence position for the private build and what a future release would need |
| `docs/BUILD_WII.md` | Building the DOL, card layout, testing on Dolphin and hardware, debugging, and the feature cuts with recommendations |
| `tools/desktop/` | Scripts that produce the baseline (`setup.sh`, `bench.sh`, `summarize.py`, `matrix.txt`) |
| `tools/wii/check_qgl.sh` | Verifies every GL entry point the frontend calls is wired in the GX build |
| `engine/` | Vendored Spearmint engine subset (`c48bdf60`) with the Wii changes; `Makefile` is the desktop build, `Makefile.wii` the DOL build |
| `engine/code/wii/` | Wii platform layer: boot, sys, memory, video, GameCube pads, sound, loopback networking |
| `engine/code/renderergx/` | Native GX backend (from ioQuake3-wii, with permission) behind `WII_NATIVE_GX` |
| `game/` | Vendored mint-arena (`482d5283`) with the memory trims; builds the QVMs with its own LCC tools |
| `data/wii.cfg` | Default binds and settings for four GameCube pads |

The desktop build of the vendored tree (`make -C engine ...` and
`make -C game ...`, see `docs/DESKTOP_BASELINE.md`) is the regression
harness: it must keep building and running four seats after every engine
change.
