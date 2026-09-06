/*
===========================================================================
wii_input.c: GameCube controller input for four seats.

Each PAD channel is one Spearmint local player. The engine already routes
joystick events per seat (SE_JOYSTICK_AXIS + seat, SE_JOYSTICK_BUTTON +
seat), the CGame VM maps them through the per-seat joystick remap to
K_JOY_* / K_2JOY_* / ... key codes, and the shared bind table does the
rest. So this file only has to poll the pads and emit events.

Written for this project; the axis filtering follows the donor port
(Mayo1970/ioQuake3-wii) and its measured dead zones.
===========================================================================
*/
#include <gccore.h>
#include <stdlib.h>

#include "wii_local.h"
#include "../client/client.h"
#include "../cgame/cg_public.h"   /* joyevent_t */

#define WII_NUM_PADS        MAX_SPLITVIEW   /* PAD_CHAN0..3 */

/* Button numbering as seen by the CGame joystick remap. */
enum {
	GCB_A, GCB_B, GCB_X, GCB_Y, GCB_Z, GCB_START,
	GCB_DPAD_UP, GCB_DPAD_RIGHT, GCB_DPAD_DOWN, GCB_DPAD_LEFT,
	GCB_L, GCB_R,               /* digital clicks of the analog triggers */
	GCB_COUNT
};

/* Axis numbering. Positive Y is "down" like SDL so the default remap matches. */
enum {
	GCA_MAIN_X, GCA_MAIN_Y, GCA_C_X, GCA_C_Y, GCA_TRIGGER_L, GCA_TRIGGER_R,
	GCA_COUNT
};

static const struct { u32 bit; int button; } s_buttonBits[] = {
	{ PAD_BUTTON_A,     GCB_A },
	{ PAD_BUTTON_B,     GCB_B },
	{ PAD_BUTTON_X,     GCB_X },
	{ PAD_BUTTON_Y,     GCB_Y },
	{ PAD_TRIGGER_Z,    GCB_Z },
	{ PAD_BUTTON_START, GCB_START },
	{ PAD_BUTTON_UP,    GCB_DPAD_UP },
	{ PAD_BUTTON_RIGHT, GCB_DPAD_RIGHT },
	{ PAD_BUTTON_DOWN,  GCB_DPAD_DOWN },
	{ PAD_BUTTON_LEFT,  GCB_DPAD_LEFT },
	{ PAD_TRIGGER_L,    GCB_L },
	{ PAD_TRIGGER_R,    GCB_R },
};

/* Dead zones in raw stick units (-128..127). The C-stick is noisier. */
#define STICK_DEADZONE     12
#define CSTICK_DEADZONE    16
#define TRIGGER_THRESHOLD  90     /* of 255 */

typedef struct {
	qboolean connected;
	u32      buttons;             /* last emitted button state (our numbering) */
	int      axis[GCA_COUNT];     /* last emitted axis values (SDL range) */
	int      startHeldFrames;
} padState_t;

static padState_t s_pads[WII_NUM_PADS];
static qboolean   s_inited = qfalse;
static int        s_eventTime;

/*
=================
GC_FilterAxis

Raw -128..127 with a dead zone, rescaled to SDL's -32768..32767 so the
CGame threshold math (in_joystickThreshold * 32767) behaves as on desktop.
=================
*/
static int GC_FilterAxis( int raw, int deadzone )
{
	int mag = abs( raw );
	int sign = raw < 0 ? -1 : 1;
	int span;

	if ( mag <= deadzone )
		return 0;
	span = 127 - deadzone;
	mag = ( mag - deadzone ) * 32767 / span;
	if ( mag > 32767 ) mag = 32767;
	return sign * mag;
}

static int GC_FilterTrigger( int raw )
{
	/* 0..255 -> 0..32767, with a small dead zone against resting noise */
	if ( raw < 20 )
		return 0;
	return ( raw - 20 ) * 32767 / 235;
}

/*
=================
Wii_Input_Init

Called from main() before Com_Init. The engine's IN_Init runs later from
GLimp_Init, when the joystick remap tables exist.
=================
*/
void Wii_Input_Init( void )
{
	PAD_Init();
	Com_Memset( s_pads, 0, sizeof( s_pads ) );
}

/*
=================
IN_SetSeatDefaults

Install the default GameCube layout for one seat through the same
CL_SetKeyForJoyEvent path the SDL layer uses for game controllers. The
remap device is shared by ident, so all four seats point at one table and
a `joyremap` edit applies to every pad, which is what a four-player couch
wants.
=================
*/
static void IN_SetSeatDefaults( int seat )
{
	static const struct { int axis; int negKey; int posKey; } axisRemap[] = {
		{ GCA_MAIN_X,    K_JOY_LEFTSTICK_LEFT,  K_JOY_LEFTSTICK_RIGHT },
		{ GCA_MAIN_Y,    K_JOY_LEFTSTICK_UP,    K_JOY_LEFTSTICK_DOWN },
		{ GCA_C_X,       K_JOY_RIGHTSTICK_LEFT, K_JOY_RIGHTSTICK_RIGHT },
		{ GCA_C_Y,       K_JOY_RIGHTSTICK_UP,   K_JOY_RIGHTSTICK_DOWN },
		{ GCA_TRIGGER_L, -1,                    K_JOY_LEFTTRIGGER },
		{ GCA_TRIGGER_R, -1,                    K_JOY_RIGHTTRIGGER },
	};
	static const struct { int button; int key; } buttonRemap[] = {
		{ GCB_A,          K_JOY_A },
		{ GCB_B,          K_JOY_B },
		{ GCB_X,          K_JOY_X },
		{ GCB_Y,          K_JOY_Y },
		{ GCB_Z,          K_JOY_RIGHTSHOULDER },
		{ GCB_START,      K_JOY_START },
		{ GCB_DPAD_UP,    K_JOY_DPAD_UP },
		{ GCB_DPAD_RIGHT, K_JOY_DPAD_RIGHT },
		{ GCB_DPAD_DOWN,  K_JOY_DPAD_DOWN },
		{ GCB_DPAD_LEFT,  K_JOY_DPAD_LEFT },
		{ GCB_L,          K_JOY_LEFTSHOULDER },
		{ GCB_R,          K_JOY_BACK },
	};
	joyevent_t event;
	int i;

	/* Loads joy-wii-gcpad.txt if the player saved one; otherwise the table is empty. */
	if ( CL_OpenJoystickRemap( seat, "GameCube Controller", "gcpad" ) ) {
		return;   /* a saved remap exists: keep it */
	}

	for ( i = 0; i < ARRAY_LEN( axisRemap ); i++ ) {
		event.type = JOYEVENT_AXIS;
		event.value.axis.num = axisRemap[i].axis;
		event.value.axis.sign = 1;
		CL_SetKeyForJoyEvent( seat, &event, axisRemap[i].posKey );
		if ( axisRemap[i].negKey != -1 ) {
			event.value.axis.sign = -1;
			CL_SetKeyForJoyEvent( seat, &event, axisRemap[i].negKey );
		}
	}
	for ( i = 0; i < ARRAY_LEN( buttonRemap ); i++ ) {
		event.type = JOYEVENT_BUTTON;
		event.value.button = buttonRemap[i].button;
		CL_SetKeyForJoyEvent( seat, &event, buttonRemap[i].key );
	}

	/* defaults are not written back to the remap file */
	CL_SetAutoJoyRemap( seat );
}

/*
=================
IN_Init / IN_Shutdown / IN_Restart: the engine's input hooks.
=================
*/
void IN_Init( void *windowData )
{
	int seat;

	(void)windowData;

	Com_DPrintf( "\n------- Input Initialization (GameCube pads) -------\n" );

	for ( seat = 0; seat < WII_NUM_PADS; seat++ ) {
		IN_SetSeatDefaults( seat );
		Com_Memset( &s_pads[seat], 0, sizeof( s_pads[seat] ) );
	}
	s_inited = qtrue;
}

void IN_Shutdown( void )
{
	int seat;

	if ( !s_inited )
		return;
	for ( seat = 0; seat < WII_NUM_PADS; seat++ ) {
		CL_CloseJoystickRemap( seat );
	}
	s_inited = qfalse;
}

void IN_Restart( void )
{
	IN_Shutdown();
	IN_Init( NULL );
}

/*
=================
IN_QueueAxis / IN_QueueButton
=================
*/
static void IN_QueueAxis( int seat, int axis, int value )
{
	if ( s_pads[seat].axis[axis] == value )
		return;
	s_pads[seat].axis[axis] = value;
	Com_QueueEvent( s_eventTime, SE_JOYSTICK_AXIS + seat, axis, value, 0, NULL );
}

static void IN_QueueButton( int seat, int button, qboolean down )
{
	u32 bit = 1u << button;
	qboolean was = ( s_pads[seat].buttons & bit ) ? qtrue : qfalse;

	if ( was == down )
		return;
	if ( down )
		s_pads[seat].buttons |= bit;
	else
		s_pads[seat].buttons &= ~bit;
	Com_QueueEvent( s_eventTime, SE_JOYSTICK_BUTTON + seat, button, down, 0, NULL );
}

static void IN_ReleaseAll( int seat )
{
	int i;
	for ( i = 0; i < GCB_COUNT; i++ )
		IN_QueueButton( seat, i, qfalse );
	for ( i = 0; i < GCA_COUNT; i++ )
		IN_QueueAxis( seat, i, 0 );
}

/*
=================
IN_CheckDropIn

Seat 1 is always the main player. A pad on channel 2..4 that presses
START while its seat is empty joins the game through the engine's own
drop-in command, exactly as typing "2dropin" in the console would.
=================
*/
static void IN_CheckDropIn( int seat, u32 held )
{
	if ( seat == 0 )
		return;
	if ( !( held & PAD_BUTTON_START ) ) {
		s_pads[seat].startHeldFrames = 0;
		return;
	}
	if ( ++s_pads[seat].startHeldFrames != 1 )
		return;                              /* edge, not level */
	if ( clc.state != CA_ACTIVE )
		return;
	if ( clc.playerNums[seat] != -1 )
		return;                              /* already in */
	Cbuf_AddText( va( "%ddropin\n", seat + 1 ) );
}

/*
=================
IN_Frame

Polls all four channels once per frame and emits changes only.
=================
*/
void IN_Frame( void )
{
	u32 connected;
	int seat, i;

	if ( !s_inited )
		return;

	s_eventTime = Sys_Milliseconds();
	connected = PAD_ScanPads();

	for ( seat = 0; seat < WII_NUM_PADS; seat++ ) {
		u32 held;
		s8  lx, ly, cx, cy;
		u8  tl, tr;

		if ( !( connected & ( 1u << seat ) ) ) {
			if ( s_pads[seat].connected ) {
				IN_ReleaseAll( seat );
				s_pads[seat].connected = qfalse;
				Com_DPrintf( "GameCube pad %d disconnected\n", seat + 1 );
			}
			continue;
		}
		if ( !s_pads[seat].connected ) {
			s_pads[seat].connected = qtrue;
			Com_DPrintf( "GameCube pad %d connected\n", seat + 1 );
		}

		held = PAD_ButtonsHeld( seat );
		lx = PAD_StickX( seat );
		ly = PAD_StickY( seat );
		cx = PAD_SubStickX( seat );
		cy = PAD_SubStickY( seat );
		tl = PAD_TriggerL( seat );
		tr = PAD_TriggerR( seat );

		for ( i = 0; i < ARRAY_LEN( s_buttonBits ); i++ ) {
			IN_QueueButton( seat, s_buttonBits[i].button,
				( held & s_buttonBits[i].bit ) ? qtrue : qfalse );
		}

		/* Sticks: up is negative Y in SDL convention */
		IN_QueueAxis( seat, GCA_MAIN_X,  GC_FilterAxis( lx, STICK_DEADZONE ) );
		IN_QueueAxis( seat, GCA_MAIN_Y, -GC_FilterAxis( ly, STICK_DEADZONE ) );
		IN_QueueAxis( seat, GCA_C_X,     GC_FilterAxis( cx, CSTICK_DEADZONE ) );
		IN_QueueAxis( seat, GCA_C_Y,    -GC_FilterAxis( cy, CSTICK_DEADZONE ) );
		IN_QueueAxis( seat, GCA_TRIGGER_L, GC_FilterTrigger( tl ) );
		IN_QueueAxis( seat, GCA_TRIGGER_R, GC_FilterTrigger( tr ) );

		IN_CheckDropIn( seat, held );
	}
}

/*
=================
Wii_Input_SetCvars

Per-seat defaults that need the cvar system: analog look on every seat,
and each seat bound to its own pad index (informational; the channel is
the seat).
=================
*/
void Wii_Input_SetCvars( void )
{
	int seat;

	for ( seat = 0; seat < WII_NUM_PADS; seat++ ) {
		Cvar_Get( Com_LocalPlayerCvarName( seat, "in_joystick" ), "1", CVAR_ARCHIVE | CVAR_LATCH );
		Cvar_Get( Com_LocalPlayerCvarName( seat, "in_joystickNo" ), va( "%d", seat ), CVAR_ARCHIVE );
		Cvar_Get( Com_LocalPlayerCvarName( seat, "in_joystickUseAnalog" ), "1", CVAR_ARCHIVE );
		Cvar_Get( Com_LocalPlayerCvarName( seat, "in_joystickThreshold" ), "0.15", CVAR_ARCHIVE );
	}
}

/* Mouse and keyboard entry points the client references. There is no
 * pointer device on a GameCube pad; menus are driven with the stick keys. */
void IN_SetMouseCursor( int localPlayerNum, int x, int y ) { (void)localPlayerNum; (void)x; (void)y; }
