/*
===========================================================================
wii_net.c: loopback-only replacement for code/qcommon/net_ip.c.

The local listen server and the local client talk over NA_LOOPBACK, which
net_chan.c services in memory (NET_SendLoopPacket / NET_GetLoopPacket,
pumped by Com_EventLoop). Nothing here ever opens a socket, and libogc's
network stack is never initialised. Everything net_ip.c exported that the
rest of the engine references is provided with loopback semantics.
===========================================================================
*/
#include "wii_local.h"
#include <unistd.h>

char *NET_ErrorString( void )
{
	return "no network on this build";
}

qboolean Sys_StringToAdr( const char *s, netadr_t *a, netadrtype_t family )
{
	(void)family;
	if ( !Q_stricmp( s, "localhost" ) || !Q_stricmp( s, "loopback" ) ) {
		Com_Memset( a, 0, sizeof( *a ) );
		a->type = NA_LOOPBACK;
		return qtrue;
	}
	a->type = NA_BAD;
	return qfalse;
}

qboolean NET_CompareBaseAdrMask( netadr_t a, netadr_t b, int netmask )
{
	(void)netmask;
	if ( a.type != b.type )
		return qfalse;
	if ( a.type == NA_LOOPBACK )
		return qtrue;
	if ( a.type == NA_BOT )
		return qtrue;
	return qfalse;
}

qboolean NET_CompareBaseAdr( netadr_t a, netadr_t b )
{
	return NET_CompareBaseAdrMask( a, b, -1 );
}

const char *NET_AdrToString( netadr_t a )
{
	static char s[64];

	if ( a.type == NA_LOOPBACK )
		Com_sprintf( s, sizeof( s ), "loopback" );
	else if ( a.type == NA_BOT )
		Com_sprintf( s, sizeof( s ), "bot" );
	else
		Com_sprintf( s, sizeof( s ), "bad" );
	return s;
}

const char *NET_AdrToStringwPort( netadr_t a )
{
	return NET_AdrToString( a );
}

qboolean NET_CompareAdr( netadr_t a, netadr_t b )
{
	if ( !NET_CompareBaseAdr( a, b ) )
		return qfalse;
	if ( a.type == NA_LOOPBACK )
		return qtrue;
	return a.port == b.port;
}

qboolean NET_IsLocalAddress( netadr_t adr )
{
	return adr.type == NA_LOOPBACK;
}

void Sys_SendPacket( int length, const void *data, netadr_t to )
{
	(void)length; (void)data; (void)to;
}

qboolean Sys_IsLANAddress( netadr_t adr )
{
	return adr.type == NA_LOOPBACK;
}

void Sys_ShowIP( void )
{
	Com_Printf( "IP: none (loopback-only build)\n" );
}

void NET_JoinMulticast6( void ) {}
void NET_LeaveMulticast6( void ) {}
void NET_Config( qboolean enableNetworking ) { (void)enableNetworking; }

void NET_Init( void )
{
	Cvar_Get( "net_enabled", "0", CVAR_LATCH | CVAR_ARCHIVE );
	Cmd_AddCommand( "net_restart", NET_Restart_f );
}

void NET_Shutdown( void ) {}

/* Called by the frame loop when it has time to spare; with no sockets to
 * select on, just yield. */
void NET_Sleep( int msec )
{
	if ( msec > 0 )
		usleep( (useconds_t)msec * 1000 );
}

void NET_Restart_f( void ) {}
