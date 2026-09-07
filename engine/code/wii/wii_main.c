/*
===========================================================================
wii_main.c: boot sequence for the Homebrew Channel DOL.

Order matters and each step is logged to <dev>:/newgame/boot.txt in
WII_DEBUG builds so a hang on hardware names the step that did not return.

Written for this project; the stack override, MEM2 bump ordering and the
power/reset handling follow Mayo1970/ioQuake3-wii (wii_main.c), used with
the author's permission.
===========================================================================
*/
#include <gccore.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wii_local.h"

/* this file uses libogc's text console, not the engine's TTY console */
#undef CON_Init

/* libogc's default stack is small; the engine recurses through the BSP
 * loader, the shader parser and the QVM compiler. 512 KB in .bss. */
static unsigned char s_mainStack[512 * 1024] __attribute__((aligned(8)));
void *__ppc_main_sp __attribute__((section(".sdata"))) = &s_mainStack[sizeof(s_mainStack)];

qboolean wii_poweroff_requested = qfalse;
static volatile int s_power_requested = 0;
static volatile int s_reset_requested = 0;

static void wii_power_cb( void )              { s_power_requested = 1; }
static void wii_reset_cb( u32 irq, void *ctx ) { (void)irq; (void)ctx; s_reset_requested = 1; }

#ifndef WII_GAMEDIR
#define WII_GAMEDIR "newgame"
#endif
#ifndef WII_BASEGAME
#define WII_BASEGAME "baseq3"
#endif
#ifndef WII_MAXFPS_STR
#define WII_MAXFPS_STR "30"
#endif

static qboolean Wii_DirExists( const char *path )
{
	struct stat st;
	return ( stat( path, &st ) == 0 && S_ISDIR( st.st_mode ) ) ? qtrue : qfalse;
}

/*
=================
Wii_MountStorage

SD first, then USB. The data device is whichever has a WII_GAMEDIR
directory; both fs_basepath and fs_homepath point there so configs and
logs sit next to the pk3s.
=================
*/
static qboolean Wii_MountStorage( void )
{
	if ( !fatInitDefault() )
		return qfalse;

	snprintf( wii_dev_root, sizeof( wii_dev_root ), "sd:/%s", WII_GAMEDIR );
	if ( Wii_DirExists( wii_dev_root ) )
		return qtrue;
	snprintf( wii_dev_root, sizeof( wii_dev_root ), "usb:/%s", WII_GAMEDIR );
	if ( Wii_DirExists( wii_dev_root ) )
		return qtrue;

	/* neither: fall back to the SD path so the error log has somewhere to go */
	snprintf( wii_dev_root, sizeof( wii_dev_root ), "sd:/%s", WII_GAMEDIR );
	return qfalse;
}

/*
=================
Wii_EarlyConsole

A text console on the preferred video mode so fatal boot errors are
visible before GX exists.
=================
*/
static void Wii_EarlyConsole( void )
{
	GXRModeObj *rmode;
	void *xfb;

	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode( NULL );
	xfb = MEM_K0_TO_K1( SYS_AllocateFramebuffer( rmode ) );
	console_init( xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ );
	VIDEO_Configure( rmode );
	VIDEO_SetNextFramebuffer( xfb );
	VIDEO_SetBlack( FALSE );
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if ( rmode->viTVMode & VI_NON_INTERLACE )
		VIDEO_WaitVSync();
}

static void Wii_Halt( const char *msg )
{
	printf( "\n  %s\n  Halted. Press the POWER or RESET button.\n", msg );
	for ( ;; ) {
		if ( s_power_requested || s_reset_requested )
			exit( 1 );
		VIDEO_WaitVSync();
	}
}

/*
=================
Wii_VideoModePrompt

Hold LEFT on pad 1 at boot for 240p NTSC, RIGHT for 264p PAL. Default is
the console's preferred mode (480i/576i), which is what four quadrants need.
=================
*/
static void Wii_VideoModePrompt( void )
{
	u32 held;

	PAD_ScanPads();
	held = PAD_ButtonsHeld( PAD_CHAN0 );
	if ( held & PAD_BUTTON_LEFT )
		wii_video_mode_choice = 1;
	else if ( held & PAD_BUTTON_RIGHT )
		wii_video_mode_choice = 2;
	else
		wii_video_mode_choice = 0;
}

int main( int argc, char *argv[] )
{
	static char cmdline[1024];
	u32 mem2_mb, hunk_mb;

	(void)argc; (void)argv;

	Wii_EarlyConsole();
	printf( "newgame (Spearmint/Wii) starting...\n" );

	SYS_SetPowerCallback( wii_power_cb );
	SYS_SetResetCallback( wii_reset_cb );

	mem2_mb = Wii_MEM2_Init();
	printf( "[wii] MEM2 bump: %u MB; malloc arena: MEM1 %u KB + MEM2 %u KB\n", (unsigned)mem2_mb,
		(unsigned)( SYS_GetArena1Size() >> 10 ), (unsigned)( SYS_GetArena2Size() >> 10 ) );

	Wii_InitMallocLock();

	if ( !Wii_MountStorage() ) {
		Wii_Halt( "No sd:/" WII_GAMEDIR " or usb:/" WII_GAMEDIR " directory found." );
	}
	printf( "[wii] data root: %s\n", wii_dev_root );

#ifdef WII_DEBUG
	{
		char path[128];
		FILE *f;
		snprintf( path, sizeof( path ), "%s/boot.txt", wii_dev_root );
		f = fopen( path, "w" );
		if ( f ) fclose( f );
		snprintf( path, sizeof( path ), "%s/diag.txt", wii_dev_root );
		f = fopen( path, "w" );
		if ( f ) fclose( f );
	}
	Wii_CrashMark( "main() started, storage mounted" );
	Wii_BootMark( "storage mounted" );
	wii_diag( "MEM2 bump %u MB; malloc arena MEM1 %u KB + MEM2 %u KB\n", (unsigned)mem2_mb,
		(unsigned)( SYS_GetArena1Size() >> 10 ), (unsigned)( SYS_GetArena2Size() >> 10 ) );
#endif

	Wii_Input_Init();
	Wii_VideoModePrompt();
	Wii_BootMark( "pads initialised" );

	Wii_Snd_Init();
	Wii_BootMark( "audio initialised" );

	/* The MEM2 bump holds the hunk, the zone (every calloc of 4 MB or more
	 * lands there, see wii_sys.c) and the JIT code buffers (2.8 MB for both
	 * mint-arena QVMs; anything more spills into malloc). Size the hunk from
	 * what is left: 32 MB on a 41 MB bump. The two VM data segments alone
	 * take 16 MB of it, so the floor is well above WII_MIN_COMHUNKMEGS. */
	hunk_mb = mem2_mb - WII_DEF_COMZONEMEGS - 3;
	if ( hunk_mb < WII_MIN_COMHUNKMEGS )
		hunk_mb = WII_MIN_COMHUNKMEGS;

	/* Keep the +set count under MAX_CONSOLE_LINES (32). Anything that can
	 * wait until after Com_Init goes through Wii_Input_SetCvars or the
	 * shipped wii.cfg instead. */
	snprintf( cmdline, sizeof( cmdline ),
		"+set fs_basepath %s "
		"+set fs_homepath %s "
		"+set fs_steampath \"\" "
		"+set fs_gogpath \"\" "
		"+set com_basegame " WII_BASEGAME " "
		"+set com_hunkMegs %u "
		"+set com_zoneMegs %u "
		"+set r_mode -1 "
		"+set r_fullscreen 1 "
		"+set r_picmip 1 "
		"+set r_texturebits 16 "
		"+set r_primitives 2 "
		"+set r_flares 0 "
		"+set r_dynamiclight 0 "
		"+set r_fastsky 0 "
		"+set r_drawSun 0 "
		"+set r_subdivisions 20 "
		"+set r_useGlFog 0 "
		"+set com_maxfps " WII_MAXFPS_STR " "
		"+set s_khz 22 "
		"+set com_soundMegs 2 "
		"+set sv_pure 0 "
		"+set net_enabled 0 "
		"+set cl_allowDownload 0 "
		"+set vm_cgameHeapMegs 2 "
		"+set vm_gameHeapMegs 5 "
#if defined(WII_VM_NATIVE) && WII_VM_NATIVE
		"+set vm_cgame 2 +set vm_game 2 "
#else
		"+set vm_cgame 1 +set vm_game 1 "
#endif
		"+set com_logfile %d "
		"+exec wii.cfg",
		wii_dev_root, wii_dev_root, (unsigned)hunk_mb, (unsigned)WII_DEF_COMZONEMEGS,
#ifdef WII_DEBUG
		2
#else
		0
#endif
	);

	Wii_BootMark( "calling Com_Init" );
	Com_Init( cmdline );
	Wii_BootMark( "Com_Init done" );

	Wii_Input_SetCvars();

	for ( ;; ) {
		if ( s_power_requested ) {
			wii_poweroff_requested = qtrue;
			Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
			s_power_requested = 0;
		}
		if ( s_reset_requested ) {
			Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
			s_reset_requested = 0;
		}
		Com_Frame();
	}

	return 0;
}
