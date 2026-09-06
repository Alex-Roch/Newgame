/*
===========================================================================
wii_snd.c: SNDDMA backend on libogc ASND for Spearmint's software mixer.

Derived from Mayo1970/ioQuake3-wii (wii_snd.c), used with the author's
permission. Spearmint's snd_main.c and snd_dma.c run unmodified on top of
this; only the DMA ring is platform code.
===========================================================================
*/
#include <asndlib.h>
#include <ogc/cache.h>
#include <string.h>
#include <malloc.h>

#include "wii_local.h"
#include "../client/snd_local.h"   /* dma_t */

#define SND_VOICE       0
#define SND_FREQ        22050
#define SND_CHANNELS    2
#define SND_SAMPLEBITS  16
#define SND_SAMPLES     2048    /* ~93 ms at 22 kHz; submission_chunk = half */
#define SND_BYTES       (SND_SAMPLES * SND_CHANNELS * (SND_SAMPLEBITS / 8))

#define ASND_OUTPUT_RATE 48000

static qboolean s_snd_init   = qfalse;
static qboolean s_asnd_ready = qfalse;
static u8      *s_buf        = NULL;

void Wii_Snd_Init( void )
{
	ASND_Init();
	ASND_Pause( 0 );
	s_asnd_ready = qtrue;
}

void Wii_Snd_Shutdown( void )
{
	if ( s_snd_init ) {
		ASND_StopVoice( SND_VOICE );
		if ( s_buf ) { free( s_buf ); s_buf = NULL; }
		s_snd_init = qfalse;
	}
	if ( s_asnd_ready ) {
		ASND_End();
		s_asnd_ready = qfalse;
	}
}

qboolean SNDDMA_Init( void )
{
	if ( !s_asnd_ready )
		return qfalse;

	s_buf = (u8 *)memalign( 32, SND_BYTES );
	if ( !s_buf )
		return qfalse;
	memset( s_buf, 0, SND_BYTES );
	DCFlushRange( s_buf, SND_BYTES );

	dma.samplebits       = SND_SAMPLEBITS;
	dma.isfloat          = 0;
	dma.speed            = SND_FREQ;
	dma.channels         = SND_CHANNELS;
	dma.samples          = SND_SAMPLES * SND_CHANNELS;
	dma.fullsamples      = SND_SAMPLES;
	dma.submission_chunk = SND_SAMPLES / 2;
	dma.buffer           = s_buf;

	/* Infinite voice: ASND loops over the whole ring; the mixer writes
	 * ahead of the read position reported by the tick counter. */
	ASND_SetInfiniteVoice( SND_VOICE, VOICE_STEREO_16BIT, SND_FREQ, 0,
	                       s_buf, SND_BYTES, 255, 255 );

	s_snd_init = qtrue;
	return qtrue;
}

int SNDDMA_GetDMAPos( void )
{
	u32 ticks, src_frames, pos;

	if ( !s_snd_init )
		return 0;

	ticks      = ASND_GetTickCounterVoice( SND_VOICE );
	src_frames = (u32)( ( (u64)ticks * SND_FREQ ) / ASND_OUTPUT_RATE );
	pos        = src_frames % (u32)SND_SAMPLES;

	return (int)( pos * (u32)SND_CHANNELS );
}

void SNDDMA_BeginPainting( void )
{
}

/* The DSP reads main memory: flush the ring after every mix or it plays stale data. */
void SNDDMA_Submit( void )
{
	if ( !s_snd_init )
		return;
	DCFlushRange( s_buf, SND_BYTES );
}

void SNDDMA_Shutdown( void )
{
	Wii_Snd_Shutdown();
}

/* VoIP capture and master gain: not available. */
void SNDDMA_StartCapture( void ) {}
int  SNDDMA_AvailableCaptureSamples( void ) { return 0; }
void SNDDMA_Capture( int samples, byte *data ) { (void)samples; (void)data; }
void SNDDMA_StopCapture( void ) {}
void SNDDMA_MasterGain( float val ) { (void)val; }
