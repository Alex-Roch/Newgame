/*
===========================================================================
wii_local.h: declarations shared by the Wii platform layer
(code/wii/*.c) and the GX renderer glue (code/renderergx/qgl_gx.c).
===========================================================================
*/
#ifndef WII_LOCAL_H
#define WII_LOCAL_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

/* Active storage root, set at boot: "sd:/newgame" or "usb:/newgame". */
extern char wii_dev_root[64];

/* Boot-time video mode choice. 0 = VIDEO_GetPreferredMode, 1 = 240p NTSC, 2 = 264p PAL. */
extern int wii_video_mode_choice;

/* wii_video.c */
qboolean Wii_Video_Init( void );
void     Wii_Video_Shutdown( void );
void     Wii_Video_EndFrame( void );
int      Wii_Video_Width( void );
int      Wii_Video_Height( void );
qboolean Wii_Video_IsPAL( void );

/* wii_sys.c */
u32      Wii_MEM2_Init( void );          /* reserves the MEM2 bump; returns its size in MB */
void    *Wii_MEM2_Alloc( size_t size );  /* 32-byte aligned bump allocation, NULL when exhausted */
u32      Wii_MEM2_Remaining( void );
void     Wii_InitMallocLock( void );
void     Wii_BootMark( const char *msg ); /* appends a line to boot.txt (WII_DEBUG only) */
void     Wii_CrashMark( const char *msg );

/* wii_snd.c */
void     Wii_Snd_Init( void );           /* ASND bring-up, before Com_Init */
void     Wii_Snd_Shutdown( void );

/* wii_input.c */
void     Wii_Input_Init( void );         /* PAD_Init, before Com_Init */
void     Wii_Input_SetCvars( void );     /* per-seat defaults that need the cvar system */

/* wii_main.c */
extern qboolean wii_poweroff_requested;

#endif /* WII_LOCAL_H */
