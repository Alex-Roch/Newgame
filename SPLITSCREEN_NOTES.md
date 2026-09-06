# How Spearmint splitscreen actually works

Everything below was read from the source trees on 2026-09-06:

| Tree | Commit | Date |
|---|---|---|
| clover-moe/spearmint | `c48bdf60` | 2026-05-25 |
| clover-moe/mint-arena | `482d5283` | 2026-04-07 |
| ioquake/ioq3 (reference) | `58839361` | 2026-07-16 |

Spearmint's merge base with ioq3 main is `d8b1769d` (2025-06-16), so it is a
recent ioq3 plus about 1700 Spearmint commits, not an old fork.

The short version: a "seat" is a *local player*. One network client
(`client_t` on the server, `clc` on the client) owns up to `MAX_SPLITVIEW`
players. The engine carries per-seat input, per-seat snapshot player states
and per-seat area visibility. Everything visual (viewport rectangles, HUD,
console, menus) lives in the CGame VM. The renderer knows nothing about
seats; it is handed one `refdef_t` per seat per frame.

## 1. The constant and the shared structures

* `MAX_SPLITVIEW 4` in `code/qcommon/q_shared.h:1251`. The client uses
  `CL_MAX_SPLITVIEW` (`code/client/client.h:49`), which defaults to
  `MAX_SPLITVIEW`. Reference counts: 22 uses in `qcommon`, 176 in `client`,
  102 in `server`, 66 in mint-arena `cgame`, 0 in `renderergl1`, 2 in
  `renderercommon` (`ri.CL_MaxSplitView` import).
* Client side (`client.h`):
  * `clientActive_t.localPlayers[CL_MAX_SPLITVIEW]` (`clientActivePlayer_t`),
    holds per-seat view angles, mouse state, etc.
  * `clientActive_t.cmdss[CL_MAX_SPLITVIEW][CMD_BACKUP]`: per-seat usercmd
    history.
  * `clientActive_t.parseEntities`: one darray sized
    `CL_MAX_SPLITVIEW * PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES`
    (`cl_cgame.c:2280`). With mint-arena's 248-byte `entityState_t` that is
    4 x 32 x 512 x 248 = 16.25 MB of zone. See PORTING_PLAN for the Wii cut.
  * `clSnapshot_t.playerNums[MAX_SPLITVIEW]`,
    `localPlayerIndex[MAX_SPLITVIEW]`, `areamask[MAX_SPLITVIEW][...]`,
    `playerStates` darray (`client.h:69-76`).
  * `clientConnection_t.playerNums[MAX_SPLITVIEW]`: server player slots for
    each seat, -1 when the seat is empty (`cl_main.c:1589`).
  * `clc.desiredPlayerBits`: bitmask of seats requested at connect.
* Server side (`server.h`):
  * `player_t` (`server.h:154`) is a per-player record (name, userinfo,
    gentity, rate...). `svs.players` is `[sv_maxclients]`.
  * `client_t.localPlayers[MAX_SPLITVIEW]` (`server.h:158`) points into
    `svs.players`. The netchan, reliable commands and snapshots are per
    `client_t`; the player state is per `player_t`.
  * `clientSnapshot_t` carries `areabits[MAX_SPLITVIEW]`,
    `localPlayerIndex[MAX_SPLITVIEW]`, `playerNums[MAX_SPLITVIEW]` and a
    `playerStates` darray of up to `MAX_SPLITVIEW` states
    (`server.h:113-118`, `sv_snapshot.c:626`).

## 2. Adding and removing seats

### At connect time

* `cl_localPlayers` (registered `cl_main.c:3713`, default `"1"`) is a
  **bitmask**, not a count: bit 0 is seat 1, bit 3 is seat 4. `15` asks for
  four seats.
* `CL_SetChallenging` (`cl_main.c:1497`) latches
  `clc.desiredPlayerBits = Com_Clamp(1, 15, cl_localPlayers)` and then resets
  the cvar to `"1"`, so it must be set before every connect (a `+set` on the
  command line before `+map` works; the local server's connect path goes
  through the same challenge state).
* `CL_CheckForResend` (`cl_main.c:2479`) builds the `connect` packet as
  `connect "<info seat1>" "<info seat2>" "<info seat3>" "<info seat4>"`,
  writing an empty string `""` for seats not requested. Each seat's userinfo
  comes from `Cvar_InfoString(cl_userinfoFlags[i])`; seat N's userinfo cvars
  are the seat-prefixed ones (`2name`, `2model`, ...; see section 7).
* Server `SV_DirectConnect` (`sv_client.c:312`) reads up to `MAX_SPLITVIEW`
  info strings (`maxLocalPlayers = Com_Clamp(1, MAX_SPLITVIEW, Cmd_Argc()-1)`,
  line 340) and calls `SV_AddPlayer(newcl, i, Cmd_Argv(1+i))` for each
  non-empty one (line 534).
* `SV_AddPlayer` (`sv_client.c:188`) finds a free `player_t`, links it into
  `client->localPlayers[localPlayerNum]` and calls
  `GAME_PLAYER_CONNECT(playerNum, firstTime, isBot, clientNum, localPlayerNum)`
  (line 275). The game VM therefore knows which seat a player is.

### During play

* Client console commands `dropin`, `dropout`, `2dropin`..`4dropout`
  (`cl_main.c:3752-3763`) call `CL_DropIn(seat)` / `CL_DropOut(seat)`, which
  just send reliable commands `dropin<N> "<userinfo>"` / `dropout<N>`
  (`cl_main.c:1820` region).
* Server handles them in the `ucmds[]` table (`sv_client.c:1603-1610`):
  `SV_DropIn1_f`..`SV_DropIn4_f` call `SV_AddPlayer(client, seat, userinfo)`,
  `SV_DropOut1_f`..`SV_DropOut4_f` call `SV_DropOut_f(client, seat)` which
  drops that `player_t` only (`sv_client.c:1435`).
* Snapshot writing (`sv_snapshot.c:632`) loops seats, skips empty
  `localPlayers[i]`, and records `frame->playerNums[i]`.
* Client parse (`cl_parse.c:321-349`) reads `localPlayerIndex` and
  `playerNums` for all four seats every snapshot, then one areamask per seat,
  then delta-decodes each present player state against the previous snapshot's
  state for that seat. `clc.playerNums[seat]` is updated from the
  `CL_SetPlayerNum`-style helpers at `cl_parse.c:72-84`.
* CGame is told about seat changes through `CG_Ingame_Init(..., maxSplitView,
  playerNum0..3)` (`cg_main.c:48`) and `cg.localPlayers[i].playerNum == -1`
  marks an empty seat.

## 3. Per-seat input: where usercmds are built

Spearmint moved usercmd construction into CGame. The engine only collects
device events and owns the send path.

* `CL_CreateNewCommands` (`cl_input.c:246`) loops seats and stores
  `cl.cmdss[i][cmdNum] = CL_CreateCmd(i)` (line 279).
* `CL_CreateCmd(localPlayerNum)` (`cl_input.c:214`) gathers mouse deltas for
  that seat via `CL_MouseMove(localPlayerNum, ...)` and calls
  `VM_Call(cgvm, CG_CREATE_USER_CMD, localPlayerNum, com_frameTime,
  frame_msec, mx, my, anykeydown)` (line 225). The VM returns a pointer to a
  `usercmd_t` inside VM memory, copied out with `VM_ExplicitArgPtr`.
* mint-arena `CG_CreateUserCmd` (`cg_input.c:574`) does the classic
  `CL_AdjustAngles` / `CL_CmdButtons` / `CL_KeyMove` / `CL_MouseMove` /
  `CL_FinishMove` sequence per seat, using `cg.localPlayers[seat]` for view
  angles and `cis[seat]` (a `clientInput_t`) for the +button states. Per-seat
  input cvars are registered with `Com_LocalPlayerCvarName(i, ...)`
  (`cg_input.c` `CG_RegisterInputCvars`): `cg_yawspeed`, `cg_pitchspeed`,
  `cg_yawspeedanalog`, `cg_pitchspeedanalog`, `cl_run`,
  `in_joystickUseAnalog`, `in_joystickThreshold`, i.e. `2cl_run`, `3cl_run`
  and so on for seats 2-4.
* `CL_WritePacket` (`cl_input.c:361`) writes a byte of `localPlayerBits`
  followed by the command backlog for each present seat (`cl.cmdss[lc][j]`,
  line 516). Server `SV_UserMove`-equivalent reads `localPlayerBits`
  (`sv_client.c:1759`) and runs `SV_PlayerThink(player, cmd)` for each seat's
  commands.

### Joystick events carry the seat number

* `sysEvent_t` types reserve one slot per seat: `SE_JOYSTICK_AXIS +
  localPlayerNum`, `SE_JOYSTICK_BUTTON + localPlayerNum`, `SE_JOYSTICK_HAT +
  localPlayerNum` (`qcommon.h:936-941`). `Com_EventLoop` (`common.c:2569`)
  turns these into `CL_JoystickAxisEvent(seat, axis, value, time)` etc.
  (`cl_joystick.c:759-790`), which forward straight to the CGame VM as
  `CG_JOYSTICK_AXIS_EVENT` / `CG_JOYSTICK_BUTTON_EVENT` /
  `CG_JOYSTICK_HAT_EVENT` with the seat as the first argument.
* mint-arena handles them in `cg_main.c:3350` (`CG_JoystickAxisEvent`) and
  `cg_main.c:3407` (`CG_JoystickButtonEvent`). Buttons resolve to per-seat
  key codes: `K_FIRST_JOY`/`K_JOY_A`.. for seat 1, `K_FIRST_2JOY`/`K_2JOY_A`..
  for seat 2, and so on (`keycodes.h:122-170`). Binds are one shared table
  keyed by those seat-specific key codes, so `bind JOY_A +jump` and
  `bind 2JOY_A +jump` are independent.
* Joystick-to-keycode remapping (`cl_joystick.c`, commands `joyremap`,
  `joyunmap`, `joyremaplist`, `joyunmapall`) is per seat:
  `CL_SetKeyForJoyEvent(localPlayerNum, joyevent, keynum)` (line 183) and
  `CL_OpenJoystickRemap(localPlayerNum, name, ident)` (line 645) load
  `joy-<ident>.txt`-style remap files per seat. `CL_SetAutoJoyRemap`
  (line 578) applies a default mapping. The SDL layer opens one device per
  seat using the seat-prefixed cvars `in_joystick`, `in_joystickNo`,
  `in_joystickThreshold` (`sdl_input.c:773,1434`), so seat N maps to SDL
  joystick index `<N>in_joystickNo`.
* Keyboard and mouse: `CL_KeyEvent(key, down, time)` has no seat argument
  (`cl_keys.c:1032`); keyboard keys always belong to seat 1. Mouse events do
  carry a seat: `CL_MouseEvent(localPlayerNum, dx, dy, time)`
  (`cl_input.c:95`) with `Mouse_GetState/Mouse_SetState(localPlayerNum, ...)`
  deciding whether the seat's mouse feeds the client (view) or CGame (UI cursor).

For the Wii, the port layer only has to emit `SE_JOYSTICK_AXIS + chan`,
`SE_JOYSTICK_BUTTON + chan` for GameCube channel `chan` 0..3. No other seat
routing exists or is needed.

## 4. Per-seat rendering

### The renderer is seat-blind

* `RE_RenderScene(fd, bufsize)` (`tr_scene.c`, `tr_public.h`) builds one
  `viewParms_t` from the refdef rectangle: `viewportX = refdef.x`,
  `viewportY = vidHeight - (refdef.y + refdef.height)`, width and height
  from the refdef (`tr_scene.c:690-693`). Nothing else in `renderergl1`
  references seats. Each seat is therefore a full `R_RenderView` with its own
  viewport and scissor, exactly as ioq3 renders a picture-in-picture.
* `RE_SetClipRegion(region)` (`tr_cmds.c:295`) is a 2D scissor used by CGame
  to clip HUD elements to a seat's rectangle.
* The colour clear happens once per frame in `RB_SwapBuffers` when `r_clear`
  is set (`tr_backend.c:1215`); `RB_BeginDrawingView` (`tr_backend.c:440`)
  clears depth for each view and colour only for `r_fastsky`. For the GX
  backend this maps to one EFB depth clear per seat viewport (the donor's
  `GXBE_Clear` draws a scissored quad, so per-seat clears are already cheap).

### CGame decides the layout

* `CG_DrawActiveFrame` (`cg_view.c:1028`). First loop (line 1079) counts
  seats with `playerNum != -1`, sets `cg.numViewports`, and runs
  `CG_PredictPlayerState` per seat. Second loop (line 1127) sets
  `cg.viewport` (0..3), `cg.cur_localPlayerNum`, `cg.cur_lc`, `cg.cur_ps`
  and calls `CG_DrawActive(stereoView)`, which calls `CG_CalcVrect` then
  `trap_R_RenderScene`.
* `CG_CalcVrect` (`cg_view.c:204`) computes the rectangle:
  * 2 seats: halves, horizontal split by default, vertical if
    `cg_splitviewVertical 1`.
  * 3 seats: quarters plus one half-size seat unless
    `cg_splitviewThirdEqual 1` (default), in which case four quarters and the
    empty quarter shows the tourney scoreboard (`cg_view.c:1214-1222`).
  * 4 seats: quarters, seat order top-left, top-right, bottom-left,
    bottom-right.
  * `cgs.screenXScale/YScale/XBias/YBias` are recomputed per viewport so all
    640x480-space HUD drawing scales into the quadrant.
  * `cg_viewsize` still applies inside each quadrant.
* After all seats, `CG_DrawScreen2D` (`cg_view.c` end of
  `CG_DrawActiveFrame`) draws full-screen overlays (console, chat, menus)
  once with `numViewports` reset to 1.
* `cg.singleCamera` (`cg_view.c:1077`): when every local player is a
  spectator following the same view, only one viewport is drawn.
* Cvars: `cg_splitviewVertical` (0), `cg_splitviewThirdEqual` (1),
  `cg_splitviewTextScale` (2) registered at `cg_main.c:423-425`.

### Console, chat and UI

* The engine console is reduced to `cl_console.c` (162 lines: log dump,
  notify clear, `CL_ConsolePrint`). The interactive console, notify lines,
  chat input and all menus live in CGame (`cg_console.c`, 853 lines;
  `cg_consolecmds.c`; the `q3_ui` and `ui` directories are compiled into the
  cgame QVM). `cg.localPlayers[i].consoleLines` holds per-seat notify text
  (`cg_view.c:1099`).

### Sound

* `S_Respatialize(entityNum, origin, axis, inwater, firstPerson)`
  (`snd_public.h:65`) is called once per seat; the mixer keeps
  `MAX_LISTENERS = MAX_SPLITVIEW + 4` listeners (`snd_local.h:187`) and
  spatialises each channel against the closest valid listener
  (`snd_dma.c:465-477`, `S_ClosestListener` at line 827). Loop sounds pick
  one listener for doppler. The per-frame channel allowance scales with
  seats (`allowed = 4 * CL_MAX_SPLITVIEW`, `snd_dma.c:569`).

## 5. Server-side visibility per seat

* `SV_BuildClientSnapshot` (`sv_snapshot.c` from line 614) builds one
  snapshot per `client_t`, containing up to four player states. For each
  seat it calls `SV_AddEntitiesVisibleFromPoint(psIndex, playerNum, origin,
  frame, &entityNumbers, portal)` (line 672) and writes that seat's
  `areabits[psIndex]` (line 411). Entities are merged into a single sorted
  list (`snapshotEntities[MAX_SNAPSHOT_ENTITIES * MAX_SPLITVIEW]`, line 323),
  so the client receives the union once, plus one area mask per seat.
* CGame checks `cg.snap->pss[seat]` for player states and uses the per-seat
  areamask (`trap_R_RenderScene` gets the refdef areamask copied from
  `cg.snap->areamask[seat]`).

## 6. Per-seat memory that scales with seats (measured)

From the desktop debug build on `oa_dm1`, hunk labels (`hunklog`):

| Allocation | 1 seat | 4 seats + 3 bots | Notes |
|---|---:|---:|---|
| `tr_model.c:495` MD3 model data | 2.8 MB | 15.8 MB | Player models; each seat defaults to a different model via `mint-game.settings` (`model`, `2model`, `3model`, `4model`). Bots add more. |
| Everything else on hunk | 81.5 MB | 81.5 MB | Unchanged with seat count. |

So in Spearmint the seat count itself costs very little engine memory. What
costs memory is the *content* loaded because four different player models
are on screen. That is a content decision, not an engine one.

## 7. Cvars and commands, complete list for seats

Engine (`code/client`, `code/server`):

* `cl_localPlayers` bitmask, read at challenge time, reset to 1 afterwards.
* `dropin`, `dropout`, `2dropin`..`4dropout` client commands.
* Server client commands `dropin1..4 "<userinfo>"`, `dropout1..4`.
* Per-seat userinfo: `Com_LocalPlayerCvarName(seat, name)` (`common.c`)
  prefixes `2`, `3`, `4` (after any `+`/`-`): `name`/`2name`/`3name`/`4name`,
  `model`, `headmodel`, `team_model`, `team_headmodel`, `color1`, `color2`,
  `handicap`, `sex`, `rate`, `snaps`, and `cl_userinfoFlags[i]` selects which
  cvars are sent for each seat.
* Per-seat input: `in_joystick`, `in_joystickNo`, `in_joystickThreshold`,
  `in_joystickUseAnalog`, `cl_run`, `cg_yawspeed`, `cg_pitchspeed`,
  `cg_yawspeedanalog`, `cg_pitchspeedanalog`, `cg_anglespeedkey` (prefixed).
* `joyremap`, `joyunmap`, `joyremaplist`, `joyunmapall` (`cl_joystick.c:549`).
* `Mouse_SetState(seat, MOUSE_CLIENT|MOUSE_CGAME)` exposed to CGame.

CGame (mint-arena):

* `cg_splitviewVertical`, `cg_splitviewThirdEqual`, `cg_splitviewTextScale`.
* Game defaults from `mint-game.settings` (`cvarDefault 2model major` etc.).

## 8. Things the Wii port does not have to invent

* Seat to device routing: `SE_JOYSTICK_* + chan` from a per-channel PAD poll.
* Viewport math: CGame.
* Per-seat HUD scaling, notify, scoreboard placement: CGame.
* Per-seat sound listeners: engine mixer.
* Drop-in and drop-out: reliable commands already exist; a "press Start on
  an unused pad" feature is one `Cbuf_AddText("2dropin\n")` away.

## 9. Things the Wii port does have to do

* Emit joystick events per PAD channel and provide a sane default
  `joyremap` (or auto remap) for the GameCube layout for all four seats.
* Write per-seat bind persistence: Spearmint already saves `bind 2JOY_A ...`
  lines into the config, so this is free once key codes are stable.
* Provide the per-seat `2model`/`3model`/`4model` defaults for whatever
  player models exist in the final content (or make all seats share one
  model to save the 13 MB measured above).
* Nothing in the renderer, provided per-view depth clears are cheap on GX
  (they are: scissored quad).
