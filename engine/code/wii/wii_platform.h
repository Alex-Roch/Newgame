/*
===========================================================================
wii_platform.h: force-included (-include) before every translation unit
of the Wii build. Supplies platform identity, endianness, memory-budget
overrides and the diagnostic log helpers.

Derived in part from Mayo1970/ioQuake3-wii (wii_platform.h), used with the
author's permission, and rewritten for Spearmint.
===========================================================================
*/
#ifndef WII_PLATFORM_H
#define WII_PLATFORM_H

#ifndef GEKKO
#define GEKKO
#endif
#ifndef WII
#define WII
#endif

/* libogc first: it defines COLOR_* macros that q_shared.h redefines as
 * colour-escape characters. Include it once here, then drop its macros so
 * the engine's win everywhere; gccore.h has include guards so later
 * includes are no-ops. */
#include <gccore.h>
#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_CYAN
#undef COLOR_MAGENTA
#undef COLOR_WHITE
#undef COLOR_ORANGE
#undef COLOR_MAROON
#undef COLOR_GRAY
#undef COLOR_SILVER
#undef COLOR_PURPLE
#undef COLOR_LIME
#undef COLOR_OLIVE
#undef COLOR_NAVY
#undef COLOR_TEAL
#undef COLOR_AQUA
#undef COLOR_FUCHSIA

/* libogc's console API also has a CON_Init; the engine's TTY console
 * (sys_local.h, con_passive.c) uses the same name. Rename the engine's. */
#define CON_Init spearmint_CON_Init
void spearmint_CON_Init( void );

/* Endianness: Broadway is big-endian. q_platform.h keys off these. */
#ifndef __BIG_ENDIAN
#  define __BIG_ENDIAN 4321
#endif
#ifndef __BYTE_ORDER
#  define __BYTE_ORDER __BIG_ENDIAN
#endif
#undef  Q3_LITTLE_ENDIAN
#define Q3_BIG_ENDIAN
#ifndef __powerpc__
#  define __powerpc__ 1      /* devkitPPC does not predefine it; q_platform.h and vm_powerpc.c test it */
#endif

#ifndef OS_STRING
#  define OS_STRING "wii"
#endif
#ifndef ARCH_STRING
#  define ARCH_STRING "ppc"
#endif
#ifndef ID_INLINE
#  define ID_INLINE __inline__
#endif
#ifndef PATH_SEP
#  define PATH_SEP '/'
#endif
#ifndef DLL_EXT
#  define DLL_EXT ".so"
#endif
#ifndef DLL_PREFIX
#  define DLL_PREFIX "lib"
#endif

/* QVM execution: native PPC JIT unless WII_VM_NATIVE=0 */
#if defined(WII_VM_NATIVE) && WII_VM_NATIVE && !defined(HAVE_VM_COMPILED)
#  define HAVE_VM_COMPILED
#endif
#if !defined(HAVE_VM_COMPILED) && !defined(NO_VM_COMPILED)
#  define NO_VM_COMPILED
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* mmap shims for vm_powerpc.c: there is no sys/mman.h on devkitPPC. The
 * functions live in wii_sys.c and hand out MEM2 slots. */
#ifndef MAP_FAILED
#  define MAP_FAILED ((void *)-1)
#endif
#ifndef PROT_READ
#  define PROT_READ     1
#  define PROT_WRITE    2
#  define PROT_EXEC     4
#  define MAP_SHARED    0x01
#  define MAP_PRIVATE   0x02
#  define MAP_ANONYMOUS 0x20
#  define MAP_ANON      MAP_ANONYMOUS
#endif
void *wii_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int   wii_munmap(void *addr, size_t len);
#define mmap   wii_mmap
#define munmap wii_munmap
/* All Wii memory is executable. */
static inline int mprotect(void *addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }

/* timersub for vm_powerpc.c's compile-time printout */
#include <sys/time.h>
#ifndef timersub
#define timersub(a, b, res) do { \
	(res)->tv_sec  = (a)->tv_sec  - (b)->tv_sec;  \
	(res)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((res)->tv_usec < 0) { (res)->tv_sec--; (res)->tv_usec += 1000000; } \
} while (0)
#endif

#define IOAPI_NO_64BIT
#define USE_LOCAL_HEADERS      /* qgl.h includes "SDL_opengl.h": served by code/wii/include */

/* No ifaddrs.h, no sockets: networking is loopback only (wii_net.c). */
#define HAVE_SA_LEN          0
#undef  HAVE_SOCKADDR_SA_LEN
#define NET_ENABLE_IPV6      0

/* ---- Memory and network budget overrides. The engine defaults assume a
 * 64-bit desktop; every value here is a knob the hunk breakdown in
 * docs/DESKTOP_BASELINE.md identified. ---- */

/* Server and client snapshot entity arrays: 8 * 16 * 256 * 248 B and
 * 4 * 16 * 256 * 248 B (8 MB + 4 MB) instead of 32 MB + 16 MB. */
#define WII_PACKET_BACKUP            16
#define WII_MAX_SNAPSHOT_ENTITIES    256

/* backEndData_t: 65536 draw surfaces is 3 MB of sort keys alone. */
#define WII_MAX_DRAWSURFS            0x4000

/* Hunk / zone floors and defaults (common.c honours these under GEKKO). */
#define WII_MIN_COMHUNKMEGS          8
#define WII_DEF_COMHUNKMEGS          32
#define WII_DEF_COMZONEMEGS          6
#define WII_MIN_COMZONEMEGS          4

/* Sound: stock s_rawsamples is MAX_RAW_STREAMS(129) * 16384 * 8 B = 16 MB. */
#ifndef MAX_RAW_STREAMS
#define MAX_RAW_STREAMS  (MAX_SPLITVIEW + 1)
#endif

#include <stdio.h>
#include <stdarg.h>

/* ---- Diagnostics. On a console with no stdout the SD card is the
 * debugger. WII_DEBUG builds keep <dev>:/newgame/diag.txt open; release
 * builds compile the calls away. ---- */
#ifdef WII_DEBUG
extern FILE *wii_diag_fp;
extern char wii_dev_root[];
static inline FILE *wii_diag_open(void) {
	if (!wii_diag_fp) {
		char path[128];
		snprintf(path, sizeof(path), "%s/diag.txt", wii_dev_root);
		wii_diag_fp = fopen(path, "a");
	}
	return wii_diag_fp;
}
static inline void wii_diag(const char *fmt, ...) __attribute__((format(printf,1,2)));
static inline void wii_diag(const char *fmt, ...) {
	/* Persistent handle: per-call fopen/fclose stalls the GX FIFO mid-frame. */
	FILE *f = wii_diag_open();
	va_list ap;
	if (!f) return;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fflush(f);
}
/* fsync so the line survives a hard crash. Heavy SD I/O: load and error paths only. */
static inline void wii_diag_sync(const char *fmt, ...) __attribute__((format(printf,1,2)));
static inline void wii_diag_sync(const char *fmt, ...) {
	FILE *f = wii_diag_open();
	va_list ap;
	if (!f) return;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fflush(f);
	fsync(fileno(f));
}
#else
static inline void wii_diag(const char *fmt, ...) { (void)fmt; }
static inline void wii_diag_sync(const char *fmt, ...) { (void)fmt; }
#endif

#endif /* WII_PLATFORM_H */
