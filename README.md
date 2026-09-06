# newgame: 4-player splitscreen FPS for retail Wii (GameCube pads only)

Private project. Engine base is Spearmint (ioquake3 fork with four-player
splitscreen), game code is mint-arena, and the Wii platform layer is
derived from Mayo1970's ioQuake3-wii with the author's permission.

Current state: recon and desktop baseline are done; no Wii code yet.

| Document | Purpose |
|---|---|
| `PORTING_PLAN.md` | Findings, corrections to the original brief, work breakdown, estimates, risk register, decisions requested |
| `SPLITSCREEN_NOTES.md` | How Spearmint's splitscreen works, by file, function and cvar |
| `docs/DESKTOP_BASELINE.md` | Reproducible Linux build of Spearmint + mint-arena and the measured memory and frame-time baseline |
| `docs/baseline/` | Raw baseline output (meminfo, hunk breakdown, symbol sizes) |
| `LICENSING_NOTE.md` | Licence position for the private build and what a future release would need |
| `tools/desktop/` | Scripts that produce the baseline (`setup.sh`, `bench.sh`, `summarize.py`, `matrix.txt`) |

Source trees are not vendored yet; `tools/desktop/setup.sh` clones the
pinned revisions.
