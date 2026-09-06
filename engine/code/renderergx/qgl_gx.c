/*
===========================================================================
qgl_gx.c: qgl* function pointer table for the native GX build, plus the
GLimp_* glue the renderergl1 frontend expects from the platform layer.

Derived from Mayo1970/ioQuake3-wii (qgl_wii.c, wii_gl_stubs.c, wii_sys.c),
used with the author's permission, and adapted to Spearmint.

In this build the frontend still calls its qgl* pointers. All of them are
wired to no-ops except the ones that carry state the GX backend needs:
Enable/Disable(GL_TEXTURE_2D, GL_POLYGON_OFFSET_FILL), PolygonOffset,
DepthRange and the three client array pointers. Everything else reaches GX
through the GXBE_* calls placed at the frontend's choke points.
===========================================================================
*/
#if defined(WII_NATIVE_GX)

#include "../renderergl1/tr_local.h"
#include "tr_gx.h"
#include "../wii/wii_local.h"

/* ---- qgl pointer storage (normally provided by sdl_glimp.c) ---- */

int qglMajorVersion = 1, qglMinorVersion = 1;
int qglesMajorVersion = 0, qglesMinorVersion = 0;

#define GLE(ret, name, ...) name##proc * qgl##name = NULL;
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
QGL_ES_1_1_PROCS;
QGL_ES_1_1_FIXED_FUNCTION_PROCS;
QGL_1_3_PROCS;
QGL_1_5_PROCS;
QGL_2_0_PROCS;
QGL_3_0_PROCS;
QGL_ARB_occlusion_query_PROCS;
QGL_ARB_framebuffer_object_PROCS;
QGL_ARB_vertex_array_object_PROCS;
QGL_EXT_direct_state_access_PROCS;
#undef GLE

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);
void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);
void (APIENTRYP qglCompressedTexImage2DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);

/* ---- no-op bodies ---- */

static void APIENTRY ngx_noop_void(void)                        {}
static void APIENTRY ngx_noop_u(GLenum a)                       { (void)a; }
static void APIENTRY ngx_noop_uu(GLenum a, GLenum b)            { (void)a;(void)b; }
static void APIENTRY ngx_noop_i(GLint a)                        { (void)a; }
static void APIENTRY ngx_noop_f(GLfloat a)                      { (void)a; }
static void APIENTRY ngx_noop_d(GLclampd a)                     { (void)a; }
static void APIENTRY ngx_noop_b(GLboolean a)                    { (void)a; }
static void APIENTRY ngx_noop_bbbb(GLboolean a, GLboolean b, GLboolean c, GLboolean d)
	{ (void)a;(void)b;(void)c;(void)d; }
static void APIENTRY ngx_noop_nu(GLsizei n, const GLuint *p)    { (void)n;(void)p; }
static void APIENTRY ngx_noop_ni(GLsizei n, GLuint *p)          { (void)n;(void)p; }
static GLenum APIENTRY ngx_noop_ret_u(void)                     { return GL_NO_ERROR; }
static const GLubyte * APIENTRY ngx_noop_ret_str(GLenum a)      { (void)a; return (const GLubyte *)""; }
static void APIENTRY ngx_noop_getintegerv(GLenum pname, GLint *params)
	{ (void)pname; if (params) *params = 0; }
static void APIENTRY ngx_noop_getbooleanv(GLenum pname, GLboolean *params)
	{ (void)pname; if (params) *params = 0; }
static void APIENTRY ngx_noop_teximage2d(GLenum tgt, GLint lvl, GLint ifmt,
	GLsizei w, GLsizei h, GLint b, GLenum fmt, GLenum tp, const GLvoid *d)
	{ (void)tgt;(void)lvl;(void)ifmt;(void)w;(void)h;(void)b;(void)fmt;(void)tp;(void)d; }
static void APIENTRY ngx_noop_texsubimage2d(GLenum tgt, GLint lvl,
	GLint xo, GLint yo, GLsizei w, GLsizei h, GLenum fmt, GLenum tp, const GLvoid *d)
	{ (void)tgt;(void)lvl;(void)xo;(void)yo;(void)w;(void)h;(void)fmt;(void)tp;(void)d; }
static void APIENTRY ngx_noop_compressed(GLenum tgt, GLint lvl, GLenum ifmt,
	GLsizei w, GLsizei h, GLint b, GLsizei size, const GLvoid *d)
	{ (void)tgt;(void)lvl;(void)ifmt;(void)w;(void)h;(void)b;(void)size;(void)d; }
static void APIENTRY ngx_noop_gettexlevel(GLenum tgt, GLint lvl, GLenum pname, GLint *params)
	{ (void)tgt;(void)lvl;(void)pname; if (params) *params = 0; }
static void APIENTRY ngx_noop_readpixels(GLint x, GLint y, GLsizei w, GLsizei h,
	GLenum fmt, GLenum tp, GLvoid *d)
	{ (void)x;(void)y;(void)w;(void)h;(void)fmt;(void)tp;(void)d; }
static void APIENTRY ngx_noop_rect(GLint x, GLint y, GLsizei w, GLsizei h)
	{ (void)x;(void)y;(void)w;(void)h; }
static void APIENTRY ngx_noop_stencil3(GLenum a, GLint b, GLuint c)
	{ (void)a;(void)b;(void)c; }
static void APIENTRY ngx_noop_stencil2(GLenum a, GLenum b, GLenum c)
	{ (void)a;(void)b;(void)c; }
static void APIENTRY ngx_noop_texparamf(GLenum a, GLenum b, GLfloat c)
	{ (void)a;(void)b;(void)c; }
static void APIENTRY ngx_noop_texparami(GLenum a, GLenum b, GLint c)
	{ (void)a;(void)b;(void)c; }
static void APIENTRY ngx_noop_alpharef(GLenum a, GLclampf b)    { (void)a;(void)b; }
static void APIENTRY ngx_noop_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
	{ (void)r;(void)g;(void)b;(void)a; }
static void APIENTRY ngx_noop_color3f(GLfloat r, GLfloat g, GLfloat b)
	{ (void)r;(void)g;(void)b; }
static void APIENTRY ngx_noop_fv(const GLfloat *v)              { (void)v; }
static void APIENTRY ngx_noop_ubv(const GLubyte *v)             { (void)v; }
static void APIENTRY ngx_noop_translatef(GLfloat x, GLfloat y, GLfloat z)
	{ (void)x;(void)y;(void)z; }
static void APIENTRY ngx_noop_frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
	GLdouble n, GLdouble f)
	{ (void)l;(void)r;(void)b;(void)t;(void)n;(void)f; }
static void APIENTRY ngx_noop_clipplane(GLenum p, const GLdouble *eq) { (void)p;(void)eq; }
static void APIENTRY ngx_noop_texcoord2f(GLfloat s, GLfloat t)  { (void)s;(void)t; }
static void APIENTRY ngx_noop_vertex2f(GLfloat x, GLfloat y)    { (void)x;(void)y; }
static void APIENTRY ngx_noop_vertex3f(GLfloat x, GLfloat y, GLfloat z)
	{ (void)x;(void)y;(void)z; }
static void APIENTRY ngx_noop_drawelements(GLenum m, GLsizei c, GLenum t, const GLvoid *i)
	{ (void)m;(void)c;(void)t;(void)i; }
static void APIENTRY ngx_noop_drawarrays(GLenum m, GLint f, GLsizei c)
	{ (void)m;(void)f;(void)c; }
static void APIENTRY ngx_noop_texenvf(GLenum t, GLenum p, GLfloat v)
	{ (void)t;(void)p;(void)v; }
static void APIENTRY ngx_noop_multitexcoord(GLenum t, GLfloat s, GLfloat v)
	{ (void)t;(void)s;(void)v; }
static void APIENTRY ngx_noop_fogf(GLenum p, GLfloat v)         { (void)p;(void)v; }
static void APIENTRY ngx_noop_fogfv(GLenum p, const GLfloat *v) { (void)p;(void)v; }
static void APIENTRY ngx_noop_fogi(GLenum p, GLint v)           { (void)p;(void)v; }
static void APIENTRY ngx_noop_lock(GLint f, GLsizei c)          { (void)f;(void)c; }
static void APIENTRY ngx_noop_copytexsub(GLenum tgt, GLint lvl,
	GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
	{ (void)tgt;(void)lvl;(void)xo;(void)yo;(void)x;(void)y;(void)w;(void)h; }
static const GLubyte * APIENTRY ngx_noop_getstringi(GLenum a, GLuint i) { (void)a;(void)i; return (const GLubyte *)""; }

/* ---- routed wrappers: forward state the GX backend needs ---- */

static void APIENTRY ngx_route_depthrange(GLclampd n, GLclampd f)
{
	GXBE_DepthRange((float)n, (float)f);
}
/* GL stride 0 means tightly packed: size * sizeof(type) */
static int ngx_eff_stride(GLint size, GLenum type, GLsizei stride)
{
	if (stride != 0)
		return (int)stride;
	return (int)size * ((type == GL_UNSIGNED_BYTE) ? 1 : 4);
}
static void APIENTRY ngx_route_vertexptr(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	GXBE_SetVertexPtr(p, ngx_eff_stride(size, type, stride));
}
static void APIENTRY ngx_route_colorptr(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	GXBE_SetColorPtr(p, ngx_eff_stride(size, type, stride));
}
static void APIENTRY ngx_route_texcoordptr(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	GXBE_SetTexCoordPtr(p, ngx_eff_stride(size, type, stride));
}
static void APIENTRY ngx_route_enable(GLenum cap)
{
	if (cap == GL_TEXTURE_2D)
		GXBE_SetTexture2DEnabled(1);
	else if (cap == GL_POLYGON_OFFSET_FILL)
		GXBE_SetPolygonOffsetEnabled(1);
	/* CLIP_PLANE0, STENCIL_TEST, FOG, BLEND, ...: handled elsewhere or unsupported on GX */
}
static void APIENTRY ngx_route_disable(GLenum cap)
{
	if (cap == GL_TEXTURE_2D)
		GXBE_SetTexture2DEnabled(0);
	else if (cap == GL_POLYGON_OFFSET_FILL)
		GXBE_SetPolygonOffsetEnabled(0);
}
static void APIENTRY ngx_route_polygonoffset(GLfloat factor, GLfloat units)
{
	GXBE_PolygonOffset((float)factor, (float)units);
}

/*
===============
QGL_InitGX

Wire every qgl* the renderergl1 frontend calls. Anything left NULL and
called would crash, so this list is checked against the frontend by
tools/wii/check_qgl.sh.
===============
*/
void QGL_InitGX(void)
{
	qglBindTexture          = (void (APIENTRY *)(GLenum,GLuint))ngx_noop_uu;
	qglBlendFunc            = (void (APIENTRY *)(GLenum,GLenum))ngx_noop_uu;
	qglClear                = (void (APIENTRY *)(GLbitfield))ngx_noop_u;
	qglClearColor           = ngx_noop_color4f;
	qglClearStencil         = ngx_noop_i;
	qglColorMask            = ngx_noop_bbbb;
	qglCopyTexSubImage2D    = ngx_noop_copytexsub;
	qglCullFace             = ngx_noop_u;
	qglDeleteTextures       = ngx_noop_nu;
	qglDepthFunc            = ngx_noop_u;
	qglDepthMask            = ngx_noop_b;
	qglDisable              = ngx_route_disable;
	qglDrawArrays           = ngx_noop_drawarrays;
	qglDrawElements         = ngx_noop_drawelements;
	qglEnable               = ngx_route_enable;
	qglFinish               = ngx_noop_void;
	qglFlush                = ngx_noop_void;
	qglFogf                 = ngx_noop_fogf;
	qglFogfv                = ngx_noop_fogfv;
	qglGenTextures          = ngx_noop_ni;
	qglGetBooleanv          = ngx_noop_getbooleanv;
	qglGetError             = ngx_noop_ret_u;
	qglGetIntegerv          = ngx_noop_getintegerv;
	qglGetString            = ngx_noop_ret_str;
	qglHint                 = (void (APIENTRY *)(GLenum,GLenum))ngx_noop_uu;
	qglLineWidth            = ngx_noop_f;
	qglPolygonOffset        = ngx_route_polygonoffset;
	qglReadPixels           = ngx_noop_readpixels;
	qglScissor              = ngx_noop_rect;
	qglStencilFunc          = ngx_noop_stencil3;
	qglStencilMask          = (void (APIENTRY *)(GLuint))ngx_noop_u;
	qglStencilOp            = ngx_noop_stencil2;
	qglTexImage2D           = ngx_noop_teximage2d;
	qglTexParameterf        = ngx_noop_texparamf;
	qglTexParameteri        = ngx_noop_texparami;
	qglTexSubImage2D        = ngx_noop_texsubimage2d;
	qglViewport             = ngx_noop_rect;

	qglAlphaFunc            = ngx_noop_alpharef;
	qglColor4f              = ngx_noop_color4f;
	qglColorPointer         = ngx_route_colorptr;
	qglDisableClientState   = ngx_noop_u;
	qglEnableClientState    = ngx_noop_u;
	qglLoadIdentity         = ngx_noop_void;
	qglLoadMatrixf          = ngx_noop_fv;
	qglMatrixMode           = ngx_noop_u;
	qglPopMatrix            = ngx_noop_void;
	qglPushMatrix           = ngx_noop_void;
	qglShadeModel           = ngx_noop_u;
	qglTexCoordPointer      = ngx_route_texcoordptr;
	qglTexEnvf              = ngx_noop_texenvf;
	qglTranslatef           = ngx_noop_translatef;
	qglVertexPointer        = ngx_route_vertexptr;

	qglClearDepth           = ngx_noop_d;
	qglDepthRange           = ngx_route_depthrange;
	qglDrawBuffer           = ngx_noop_u;
	qglPolygonMode          = (void (APIENTRY *)(GLenum,GLenum))ngx_noop_uu;
	qglPointSize            = ngx_noop_f;

	qglArrayElement         = ngx_noop_i;
	qglBegin                = ngx_noop_u;
	qglClipPlane            = ngx_noop_clipplane;
	qglColor3f              = ngx_noop_color3f;
	qglColor3fv             = ngx_noop_fv;
	qglColor4ubv            = ngx_noop_ubv;
	qglEnd                  = ngx_noop_void;
	qglFogi                 = ngx_noop_fogi;
	qglFrustum              = ngx_noop_frustum;
	qglOrtho                = ngx_noop_frustum;
	qglTexCoord2f           = ngx_noop_texcoord2f;
	qglTexCoord2fv          = ngx_noop_fv;
	qglVertex2f             = ngx_noop_vertex2f;
	qglVertex3f             = ngx_noop_vertex3f;
	qglVertex3fv            = ngx_noop_fv;

	qglGetTexLevelParameteriv = ngx_noop_gettexlevel;
	qglCompressedTexImage2DARB = ngx_noop_compressed;
	qglGetStringi           = ngx_noop_getstringi;

	qglActiveTextureARB       = ngx_noop_u;   /* non-NULL: frontend treats multitexture as available */
	qglClientActiveTextureARB = ngx_noop_u;
	qglMultiTexCoord2fARB     = ngx_noop_multitexcoord;
	qglLockArraysEXT          = ngx_noop_lock; /* non-NULL: R_DrawElements picks the DrawElements path */
	qglUnlockArraysEXT        = ngx_noop_void;
}

/*
===============
GLimp_*: the platform side of the renderer, normally sdl_glimp.c.
===============
*/
void GLimp_Init( qboolean fixedFunction )
{
	(void)fixedFunction;

	/* The GX FIFO is torn down in GLimp_Shutdown on every vid_restart;
	 * re-init or the write-gather pipe points at freed memory. */
	if ( !Wii_Video_Init() ) {
		ri.Error( ERR_FATAL, "GLimp_Init: Wii_Video_Init failed" );
	}

	QGL_InitGX();

	glConfig.vidWidth      = Wii_Video_Width();
	glConfig.vidHeight     = Wii_Video_Height();
	glConfig.windowAspect  = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	glConfig.displayWidth  = glConfig.vidWidth;
	glConfig.displayHeight = glConfig.vidHeight;
	glConfig.displayAspect = glConfig.windowAspect;
	glConfig.displayFrequency = Wii_Video_IsPAL() ? 50 : 60;
	glConfig.isFullscreen  = qtrue;
	glConfig.colorBits     = 24;
	glConfig.depthBits     = 24;
	glConfig.stencilBits   = 0;
	glConfig.deviceSupportsGamma    = qfalse;
	glConfig.textureCompression     = TC_NONE;
	glConfig.textureEnvAddAvailable = qtrue;   /* GX TEV add stage in gxbe_set_stage0_env */
	glConfig.textureFilterAnisotropic = qfalse;
	glConfig.maxAnisotropy = 1;
	glConfig.numTextureUnits = 2;
	glConfig.stereoEnabled = qfalse;
	glConfig.maxTextureSize = 1024;            /* GX limit */
	Q_strncpyz( glConfig.renderer_string, "Nintendo GX (native)", sizeof( glConfig.renderer_string ) );
	Q_strncpyz( glConfig.vendor_string,   "Nintendo",             sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.version_string,  "1.1 GX",               sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string,
		"GL_ARB_multitexture GL_EXT_compiled_vertex_array GL_EXT_texture_env_add",
		sizeof( glConfig.extensions_string ) );

	ri.CL_GlconfigChanged( &glConfig );

	/* the SDL layer starts input from here too; the pad layer needs the
	 * joystick remap tables that exist by now */
	ri.IN_Init( NULL );
}

void GLimp_Shutdown( void )
{
	ri.IN_Shutdown();
	Wii_Video_Shutdown();
}

void GLimp_EndFrame( void )
{
	Wii_Video_EndFrame();   /* copies EFB to XFB and drains the GP */
	GXBE_FrameEnd();        /* GP idle: reset the staging ring */
}

void GLimp_LogComment( char *comment )
{
	(void)comment;
}

void GLimp_Minimize( void )
{
}

void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	(void)red; (void)green; (void)blue;
}

qboolean GLimp_ResizeWindow( int width, int height )
{
	(void)width; (void)height;
	return qfalse;
}

#endif /* WII_NATIVE_GX */
