/*
===========================================================================
Native GX backend for Spearmint renderergl1 on the Nintendo Wii.

Derived from Mayo1970/ioQuake3-wii (tr_gx.h), used with the author's
permission, and adapted to Spearmint's renderergl1 frontend.

Active only when WII_NATIVE_GX is defined. The renderergl1 frontend keeps
calling its qgl* function pointers; in this build every one of those is a
no-op except the seven routed ones in qgl_gx.c, and the real work happens
through the GXBE_* entry points below, called from #if WII_NATIVE_GX arms at
the frontend's choke points (GL_State, GL_Bind, SetViewportAndScissor,
RB_BeginDrawingView, R_DrawElements, Upload32, ...).
===========================================================================
*/
#ifndef TR_GX_H
#define TR_GX_H

#if defined(WII_NATIVE_GX)

#include <gccore.h>   /* GXTexObj, Mtx, Mtx44, u8, f32, GXBool, ... */

/* Shadow state for the native GX backend: mirrors glState_t so redundant
 * GX register writes can be skipped. */
typedef struct {
	int      currenttmu;        /* 0 or 1, kept in sync with glState.currenttmu */
	int      boundtex[2];       /* texnum bound to GX_TEXMAP0/1; -1 = none */
	int      texenv[2];         /* pending GL_MODULATE etc. per TMU */
	int      faceCulling;       /* last CT_* value; -1 = force-set next call */
	int      numActiveTMUs;     /* 1 or 2, driven by qglEnable/Disable(GL_TEXTURE_2D) on TMU1 */
	float    depthNear;         /* depth range near [0,1] for GX_SetViewport */
	float    depthFar;          /* depth range far  [0,1] */
	int      vpX, vpY, vpW, vpH; /* current viewport (GX top-left coords) */
	qboolean tevDirty;          /* needs GX TEV stage recommit before next draw */
	int      vtxDescNumTex;     /* last-set vtx-desc TMU count: 1, 2, or -1=unset */
	qboolean alphaTestActive;   /* last GX_SetZCompLoc state */

	/* Client vertex arrays: DrawTess reads these; the stage iterators
	 * retarget them through the routed qgl*Pointer wrappers. */
	const void *posPtr;         /* xyz array (stride 16 in all Q3 paths) */
	const void *clrPtr;         /* color4ub array */
	const void *texPtr[2];      /* texcoord array per TMU */
	int      posStride;
	int      clrStride;
	int      texStride[2];

	/* Last-loaded matrices; GXBE_Clear restores them after its clear quad. */
	Mtx44    projMtx;
	Mtx      mvMtx;
	u8       projType;          /* GX_PERSPECTIVE or GX_ORTHOGRAPHIC */
} gx_state_t;

extern gx_state_t gxState;

/* Texture table, indexed by image->texnum (0-based counter, not a GL name). */
#define GX_MAX_TEXOBJS  2048  /* must be >= MAX_DRAWIMAGES */

extern GXTexObj  s_gx_texobjs[GX_MAX_TEXOBJS];
extern qboolean  s_gx_texobj_valid[GX_MAX_TEXOBJS];
extern void     *s_gx_texbufs[GX_MAX_TEXOBJS]; /* memalign'd swizzled data */
extern int       s_gx_next_texnum;

/* Init */
void GXBE_SetDefaultState(void);
void GXBE_FrameEnd(void);          /* called from GLimp_EndFrame after GX_DrawDone */
void GXBE_Finish(void);            /* qglFinish equivalent: drains the GP */

/* State */
void GXBE_GL_State(unsigned long stateBits);
void GXBE_GL_Cull(int cullType);
void GXBE_GL_TexEnv(int unit, int env);
void GXBE_BindTexnum(int tmu, int texnum);

/* Matrices / viewport (2D path) */
void GXBE_LoadOrtho2D(int vidWidth, int vidHeight);
void GXBE_LoadIdentityModelview(void);
void GXBE_SetViewport(int x, int y, int w, int h);
void GXBE_SetScissor(int x, int y, int w, int h);
void GXBE_DepthRange(float n, float f);

/* Polygon offset for decals. factor is ignored. */
void GXBE_PolygonOffset(float factor, float units);
void GXBE_SetPolygonOffsetEnabled(int enabled);

/* Matrices (3D path). Inputs are Q3's column-major GL float[16]. */
void GXBE_LoadProjectionGL(const float *gl16);
void GXBE_LoadProjectionObliqueGL(const float *gl16, const float *eyePlane);
void GXBE_LoadModelviewGL(const float *gl16);
void GXBE_LoadModelviewTranslatedGL(const float *gl16, const vec3_t origin);

/* GX has no glClear: draws a viewport-covering quad, then restores state. */
void GXBE_Clear(qboolean clearColor, qboolean clearDepth, float r, float g, float b);

/* Immediate-mode emulation for sky/cinematic/debug paths.
 * numVerts MUST match the emitted count exactly or the GP hangs. */
void GXBE_ImmediateBegin(int primGL, int numVerts);
void GXBE_ImmediateTexVertex(const float *st, const float *xyz,
                             byte r, byte g, byte b, byte a);
void GXBE_ImmediateEnd(void);

/* Client array pointers (called from the routed qgl wrappers) */
void GXBE_SetVertexPtr(const void *p, int stride);
void GXBE_SetColorPtr(const void *p, int stride);
void GXBE_SetTexCoordPtr(const void *p, int stride);  /* applies to gxState.currenttmu */
void GXBE_SetTexture2DEnabled(int enabled);           /* applies to gxState.currenttmu */

/* Draw */
void GXBE_DrawTess(int numIndexes, const glIndex_t *indexes);

/* EFB readbacks. PeekDepth returns GL-equivalent window depth for RB_TestFlare.
 * ReadPixelsRGB de-tiles an EFB copy into packed GL bottom-up RGB rows. Both sync the GP. */
float GXBE_PeekDepth(int x, int y);
void  GXBE_ReadPixelsRGB(int x, int y, int w, int h, int padlen, byte *dst);

/* Texture ops (tr_gx_texture.c). Upload32 expects the final scaled/picmipped
 * RGBA buffer, stored as RGB565 or RGB5A3. When mipmap is true, data is
 * reduced IN PLACE level by level with R_MipMap. */
void     GXBE_CreateTexnum(GLuint *texnum);
void     GXBE_DeleteTexnum(int texnum);
void     GXBE_Upload32(unsigned *data, int width, int height,
                       int glInternalFormat, int texnum, int wrapClampMode,
                       qboolean mipmap);
void     GXBE_TexSubImage2D(int texnum, int fullW, int fullH, const byte *data);

/* Frontend hook exported from tr_image.c for the in-place mip chain. */
void R_MipMap( byte *in, int width, int height );

#endif /* WII_NATIVE_GX */

#endif /* TR_GX_H */
