/*
===========================================================================
wii_sys.c: Spearmint's Sys_* services on libogc, plus the MEM2 bump
allocator and the JIT code-buffer slots.

Written against Spearmint's sys API (code/qcommon/qcommon.h,
code/sys/sys_local.h). The memory layout and several service bodies
follow Mayo1970/ioQuake3-wii (wii_sys.c), used with the author's
permission.
===========================================================================
*/
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/mutex.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#include "wii_local.h"

/* this file uses libogc's text console, not the engine's TTY console */
#undef CON_Init

#ifdef WII_DEBUG
FILE *wii_diag_fp = NULL;
#endif

char wii_dev_root[64] = "sd:/newgame";

/*
==============================================================================
MEMORY

MEM1 (24 MB): stack, libogc, malloc arena (sbrk), GX FIFO, XFBs.
MEM2 (64 MB minus the IOS reservation): a bump region taken off the top of
Arena2 at boot. The hunk lives there (com_hunkMegs = bump - 1), and so do
the JIT code buffers. Everything else uses the libogc malloc arena.
==============================================================================
*/
static u8  *s_mem2_base = NULL;
static u8  *s_mem2_ptr  = NULL;
static u32  s_mem2_left = 0;

#define WII_MEM2_BUMP_MAX   (48u * 1024u * 1024u)
/* Left in Arena2 for malloc (GX textures live there; MEM1 adds about 10 MB
 * more). A 51 MB Arena2 (retail Wii and Dolphin) gives a 41 MB bump. */
#define WII_MEM2_RESERVE    (10u * 1024u * 1024u)

u32 Wii_MEM2_Init( void )
{
	u8 *lo = (u8 *)SYS_GetArena2Lo();
	u8 *hi = (u8 *)SYS_GetArena2Hi();
	u32 total = (u32)( hi - lo );
	u32 bump  = ( total > WII_MEM2_RESERVE ) ? total - WII_MEM2_RESERVE : 0;

	if ( bump > WII_MEM2_BUMP_MAX )
		bump = WII_MEM2_BUMP_MAX;
	bump &= ~31u;

	s_mem2_base = hi - bump;
	s_mem2_ptr  = s_mem2_base;
	s_mem2_left = bump;

	SYS_SetArena2Hi( s_mem2_base );

	return bump >> 20;
}

void *Wii_MEM2_Alloc( size_t size )
{
	size_t aligned = ( size + 31 ) & ~(size_t)31;

	if ( s_mem2_ptr && aligned <= s_mem2_left ) {
		void *p = s_mem2_ptr;
		s_mem2_ptr  += aligned;
		s_mem2_left -= aligned;
		return p;
	}
	return NULL;
}

u32 Wii_MEM2_Remaining( void )
{
	return s_mem2_left;
}

static inline int is_mem2_ptr( const void *p )
{
	return s_mem2_base != NULL && (const u8 *)p >= s_mem2_base;
}

/* Large calloc requests (the hunk, the zone) come from the MEM2 bump so
 * the malloc arena stays free for the many small allocations. */
extern void *__real_calloc( size_t nmemb, size_t size );
void *__wrap_calloc( size_t nmemb, size_t size )
{
	size_t total = nmemb * size;

	if ( total >= 4u * 1024u * 1024u ) {
		void *p = Wii_MEM2_Alloc( total );
		if ( p ) {
			memset( p, 0, total );
			return p;
		}
	}
	return __real_calloc( nmemb, size );
}

extern void __real_free( void *p );
void __wrap_free( void *p )
{
	/* MEM2 bump memory is never returned; the engine frees the zone on
	 * restart and calloc's it again, which lands back on the bump. */
	if ( p && is_mem2_ptr( p ) )
		return;
	__real_free( p );
}

/* JIT code buffers: vm_powerpc.c mmaps one per VM and munmaps it on
 * VM_Free. A small slot table recycles them across map changes so the
 * bump is not consumed on every restart. */
#define WII_VMCODE_SLOTS 8
static struct {
	u8  *ptr;
	u32  size;
	int  used;
} s_vmcode[WII_VMCODE_SLOTS];

void *wii_mmap( void *addr, size_t len, int prot, int flags, int fd, off_t offset )
{
	int i, best = -1;

	(void)addr; (void)prot; (void)flags; (void)fd; (void)offset;

	/* best-fit recycle */
	for ( i = 0; i < WII_VMCODE_SLOTS; i++ ) {
		if ( s_vmcode[i].ptr && !s_vmcode[i].used && s_vmcode[i].size >= len &&
		     ( best < 0 || s_vmcode[i].size < s_vmcode[best].size ) )
			best = i;
	}
	if ( best >= 0 ) {
		s_vmcode[best].used = 1;
		wii_diag_sync( "mmap: len=%u recycled slot %d (size=%u)\n", (unsigned)len, best, s_vmcode[best].size );
		return s_vmcode[best].ptr;
	}

	for ( i = 0; i < WII_VMCODE_SLOTS; i++ ) {
		if ( !s_vmcode[i].ptr ) {
			void *p = Wii_MEM2_Alloc( len );
			if ( !p )
				break;
			s_vmcode[i].ptr  = p;
			s_vmcode[i].size = (u32)len;
			s_vmcode[i].used = 1;
			wii_diag_sync( "mmap: len=%u bump slot %d, %u MB left\n", (unsigned)len, i, Wii_MEM2_Remaining() >> 20 );
			return p;
		}
	}

	{
		void *p = memalign( 32, len );
		wii_diag_sync( "mmap: len=%u memalign -> %p\n", (unsigned)len, p );
		return p ? p : MAP_FAILED;
	}
}

int wii_munmap( void *addr, size_t len )
{
	int i;

	(void)len;
	for ( i = 0; i < WII_VMCODE_SLOTS; i++ ) {
		if ( s_vmcode[i].ptr == addr ) {
			s_vmcode[i].used = 0;
			return 0;
		}
	}
	if ( !is_mem2_ptr( addr ) )
		free( addr );
	return 0;
}

/* newlib's malloc is not thread safe by default; ASND and the pad
 * callbacks run on other threads. */
static mutex_t s_malloc_mtx = LWP_MUTEX_NULL;

void Wii_InitMallocLock( void )
{
	if ( s_malloc_mtx == LWP_MUTEX_NULL )
		LWP_MutexInit( &s_malloc_mtx, true );
}

void __wrap___malloc_lock( struct _reent *r )
{
	(void)r;
	if ( s_malloc_mtx == LWP_MUTEX_NULL )
		LWP_MutexInit( &s_malloc_mtx, true );
	LWP_MutexLock( s_malloc_mtx );
}

void __wrap___malloc_unlock( struct _reent *r )
{
	(void)r;
	if ( s_malloc_mtx != LWP_MUTEX_NULL )
		LWP_MutexUnlock( s_malloc_mtx );
}

/*
==============================================================================
BOOT AND CRASH LOGS
==============================================================================
*/
void Wii_BootMark( const char *msg )
{
#ifdef WII_DEBUG
	char path[128];
	FILE *f;

	snprintf( path, sizeof( path ), "%s/boot.txt", wii_dev_root );
	f = fopen( path, "a" );
	if ( f ) {
		fprintf( f, "[%8d ms] %s\n", Sys_Milliseconds(), msg );
		fflush( f );
		fsync( fileno( f ) );
		fclose( f );
	}
#else
	(void)msg;
#endif
}

void Wii_CrashMark( const char *msg )
{
#ifdef WII_DEBUG
	char path[128];
	FILE *f;

	snprintf( path, sizeof( path ), "%s/crash.txt", wii_dev_root );
	f = fopen( path, "w" );
	if ( f ) {
		fprintf( f, "%s\n", msg );
		fclose( f );
	}
#else
	(void)msg;
#endif
}

/*
==============================================================================
SYS SERVICES
==============================================================================
*/
void Sys_Init( void )
{
	Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
}

void Sys_Quit( void )
{
	Wii_Snd_Shutdown();
	Wii_Video_Shutdown();
	if ( wii_poweroff_requested ) {
		SYS_ResetSystem( SYS_POWEROFF, 0, 0 );
	}
	exit( 0 );   /* returns to the Homebrew Channel */
}

/* Base subtraction keeps the value positive and small; without it the
 * cast overflows after a while and the client stalls. */
int Sys_Milliseconds( void )
{
	static u64      s_base_ticks = 0;
	static qboolean s_base_set   = qfalse;

	if ( !s_base_set ) {
		s_base_ticks = gettime();
		s_base_set   = qtrue;
	}
	return (int)ticks_to_millisecs( gettime() - s_base_ticks );
}

void Sys_Sleep( int msec )
{
	if ( msec > 0 )
		usleep( (useconds_t)msec * 1000 );
}

qboolean Sys_RandomBytes( byte *string, int len )
{
	static unsigned int seed = 0;
	int i;

	if ( !seed ) {
		u64 ticks = gettime();
		seed = (unsigned int)( ticks ^ ( ticks >> 32 ) ^ (unsigned int)time( NULL ) );
		if ( !seed ) seed = 1;
	}
	for ( i = 0; i < len; i++ ) {
		seed = seed * 1664525u + 1013904223u;
		string[i] = (byte)( seed >> 24 );
	}
	return qtrue;
}

void Sys_Print( const char *msg )
{
#ifdef WII_DEBUG
	wii_diag( "%s", msg );
#else
	(void)msg;
#endif
}

void Sys_Error( const char *error, ... )
{
	va_list ap;
	char msg[4096];

	va_start( ap, error );
	Q_vsnprintf( msg, sizeof( msg ), error, ap );
	va_end( ap );

	wii_diag_sync( "Sys_Error: %s\n", msg );
	Wii_CrashMark( msg );

	/* Nothing to draw with here: put the text on the libogc console and
	 * wait for START on any pad, then return to the Homebrew Channel. */
	Wii_Video_Shutdown();
	{
		GXRModeObj *rmode = VIDEO_GetPreferredMode( NULL );
		void *xfb = MEM_K0_TO_K1( SYS_AllocateFramebuffer( rmode ) );
		console_init( xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ );
		VIDEO_Configure( rmode );
		VIDEO_SetNextFramebuffer( xfb );
		VIDEO_SetBlack( FALSE );
		VIDEO_Flush();
		VIDEO_WaitVSync();
		printf( "\n\n  FATAL ERROR\n\n  %s\n\n  Press START to exit.\n", msg );
	}
	for ( ;; ) {
		int i;
		PAD_ScanPads();
		for ( i = 0; i < 4; i++ ) {
			if ( PAD_ButtonsDown( i ) & PAD_BUTTON_START )
				exit( 1 );
		}
		VIDEO_WaitVSync();
	}
}

dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	wii_diag_sync( "Sys_Dialog(%d): %s: %s\n", (int)type, title ? title : "", message ? message : "" );
	return DR_OK;
}

void Sys_SetErrorText( const char *text ) { (void)text; }
void Sys_AnsiColorPrint( const char *msg ) { Sys_Print( msg ); }
void Sys_DisplaySystemConsole( qboolean show ) { (void)show; }
char *Sys_ConsoleInput( void ) { return NULL; }
char *Sys_GetClipboardData( void ) { return NULL; }
qboolean Sys_GetCapsLockMode( void ) { return qfalse; }
qboolean Sys_GetNumLockMode( void ) { return qfalse; }
cpuFeatures_t Sys_GetProcessorFeatures( void ) { return (cpuFeatures_t)0; }
qboolean Sys_LowPhysicalMemory( void ) { return qtrue; }
void Sys_SetEnv( const char *name, const char *value ) { (void)name; (void)value; }
char *Sys_GetCurrentUser( void ) { return "player"; }
void Sys_GLimpInit( void ) {}
void Sys_GLimpSafeInit( void ) {}
int Sys_PID( void ) { return 1; }
qboolean Sys_PIDIsRunning( int pid ) { (void)pid; return qfalse; }
void Sys_InitPIDFile( const char *gamedir ) { (void)gamedir; }
void Sys_RemovePIDFile( const char *gamedir ) { (void)gamedir; }

/* No shared libraries: every module is a QVM. */
void *QDECL Sys_LoadGameDll( const char *name, vmMainProc *entryPoint,
                             intptr_t (QDECL *systemcalls)(intptr_t, ...) )
{
	(void)name; (void)entryPoint; (void)systemcalls;
	return NULL;
}
void Sys_UnloadDll( void *dllHandle ) { (void)dllHandle; }
void *Sys_LoadDll( const char *name, qboolean useSystemLib ) { (void)name; (void)useSystemLib; return NULL; }
void *Sys_LoadLibrary( const char *f ) { (void)f; return NULL; }
void Sys_UnloadLibrary( void *h ) { (void)h; }
void *Sys_LoadFunction( void *h, const char *fn ) { (void)h; (void)fn; return NULL; }
const char *Sys_LibraryError( void ) { return "no dynamic libraries on Wii"; }
qboolean Sys_DllExtension( const char *name ) { (void)name; return qfalse; }

/*
==============================================================================
FILESYSTEM
==============================================================================
*/
static char s_installPath[MAX_OSPATH];
static char s_homePath[MAX_OSPATH];

void Sys_SetDefaultInstallPath( const char *path ) { Q_strncpyz( s_installPath, path, sizeof( s_installPath ) ); }
char *Sys_DefaultInstallPath( void ) { return s_installPath[0] ? s_installPath : wii_dev_root; }
void Sys_SetDefaultHomePath( const char *path ) { Q_strncpyz( s_homePath, path, sizeof( s_homePath ) ); }
char *Sys_DefaultHomePath( void ) { return s_homePath[0] ? s_homePath : wii_dev_root; }
char *Sys_DefaultAppPath( void ) { return wii_dev_root; }

qboolean Sys_PathIsAbsolute( const char *path )
{
	/* "sd:/..." and "usb:/..." are absolute on libfat */
	return ( path[0] == '/' || strchr( path, ':' ) != NULL ) ? qtrue : qfalse;
}

char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];
	if ( !getcwd( cwd, sizeof( cwd ) ) )
		Q_strncpyz( cwd, wii_dev_root, sizeof( cwd ) );
	return cwd;
}

const char *Sys_Basename( const char *path )
{
	const char *base = strrchr( path, '/' );
	return base ? base + 1 : path;
}

const char *Sys_Dirname( const char *path )
{
	static char dir[MAX_OSPATH];
	char *slash;

	Q_strncpyz( dir, path, sizeof( dir ) );
	slash = strrchr( dir, '/' );
	if ( slash )
		*slash = '\0';
	else
		dir[0] = '\0';
	return dir;
}

FILE *Sys_FOpen( const char *ospath, const char *mode )
{
	struct stat st;

	/* libfat lets fopen() succeed on a directory; the engine expects failure */
	if ( stat( ospath, &st ) == 0 && S_ISDIR( st.st_mode ) )
		return NULL;
	return fopen( ospath, mode );
}

int Sys_Remove( const char *ospath )
{
	return remove( ospath );
}

qboolean Sys_Mkdir( const char *path )
{
	int result = mkdir( path, 0777 );

	if ( result != 0 )
		return errno == EEXIST;
	return qtrue;
}

qboolean Sys_Rmdir( const char *path )
{
	return rmdir( path ) == 0;
}

FILE *Sys_Mkfifo( const char *ospath )
{
	(void)ospath;
	return NULL;
}

int Sys_StatFile( char *ospath )
{
	struct stat st;

	if ( stat( ospath, &st ) == -1 )
		return -1;
	if ( S_ISDIR( st.st_mode ) )
		return 1;
	return 0;
}

#define MAX_FOUND_FILES 0x1000

static void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
	char          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char          filename[MAX_OSPATH];
	DIR          *fdir;
	struct dirent *d;
	struct stat   st;

	if ( *numfiles >= MAX_FOUND_FILES - 1 )
		return;

	if ( strlen( subdirs ) )
		Com_sprintf( search, sizeof( search ), "%s/%s", basedir, subdirs );
	else
		Com_sprintf( search, sizeof( search ), "%s", basedir );

	if ( ( fdir = opendir( search ) ) == NULL )
		return;

	while ( ( d = readdir( fdir ) ) != NULL ) {
		Com_sprintf( filename, sizeof( filename ), "%s/%s", search, d->d_name );
		if ( stat( filename, &st ) == -1 )
			continue;

		if ( st.st_mode & S_IFDIR ) {
			if ( Q_stricmp( d->d_name, "." ) && Q_stricmp( d->d_name, ".." ) ) {
				if ( strlen( subdirs ) )
					Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s/%s", subdirs, d->d_name );
				else
					Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s", d->d_name );
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 )
			break;
		Com_sprintf( filename, sizeof( filename ), "%s/%s", subdirs, d->d_name );
		if ( !Com_FilterPath( filter, filename, qfalse ) )
			continue;
		list[*numfiles] = CopyString( filename );
		( *numfiles )++;
	}
	closedir( fdir );
}

char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
	struct dirent *d;
	DIR           *fdir;
	qboolean       dironly = wantsubs;
	char           search[MAX_OSPATH];
	int            nfiles;
	char         **listCopy;
	char          *list[MAX_FOUND_FILES];
	int            i;
	struct stat    st;
	int            extLen;

	if ( filter ) {
		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[nfiles] = NULL;
		*numfiles = nfiles;
		if ( !nfiles )
			return NULL;

		listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0; i < nfiles; i++ )
			listCopy[i] = list[i];
		listCopy[i] = NULL;
		return listCopy;
	}

	if ( !extension )
		extension = "";

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	}

	extLen = strlen( extension );

	if ( ( fdir = opendir( directory ) ) == NULL ) {
		*numfiles = 0;
		return NULL;
	}

	nfiles = 0;
	while ( ( d = readdir( fdir ) ) != NULL ) {
		Com_sprintf( search, sizeof( search ), "%s/%s", directory, d->d_name );
		if ( stat( search, &st ) == -1 )
			continue;
		if ( ( dironly && !( st.st_mode & S_IFDIR ) ) ||
		     ( !dironly && ( st.st_mode & S_IFDIR ) ) )
			continue;

		if ( *extension ) {
			if ( strlen( d->d_name ) < extLen ||
			     Q_stricmp( d->d_name + strlen( d->d_name ) - extLen, extension ) )
				continue;
		}

		if ( nfiles == MAX_FOUND_FILES - 1 )
			break;
		list[nfiles] = CopyString( d->d_name );
		nfiles++;
	}
	list[nfiles] = NULL;
	closedir( fdir );

	*numfiles = nfiles;
	if ( !nfiles )
		return NULL;

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0; i < nfiles; i++ )
		listCopy[i] = list[i];
	listCopy[i] = NULL;
	return listCopy;
}

void Sys_FreeFileList( char **list )
{
	int i;

	if ( !list )
		return;
	for ( i = 0; list[i]; i++ )
		Z_Free( list[i] );
	Z_Free( list );
}
