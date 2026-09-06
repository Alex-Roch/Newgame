/*
 * Wii build shim: Spearmint's qgl.h includes "SDL_opengl.h" for the GL
 * typedefs and enums. There is no SDL on the Wii; the fixed-function
 * renderer only needs the types and constants, which come from Mesa's
 * GL/gl.h and GL/glext.h (MIT licence) vendored next to this file.
 * No GL function is ever called on the Wii: every qgl* pointer is wired
 * to a no-op or to the GX backend in code/renderergx/qgl_gx.c.
 */
#ifndef WII_SDL_OPENGL_SHIM_H
#define WII_SDL_OPENGL_SHIM_H

#define GL_GLEXT_PROTOTYPES 0
#define GLAPI
#define GLAPIENTRY
#define GLAPIENTRYP *
#include <GL/gl.h>
#include <GL/glext.h>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#endif /* WII_SDL_OPENGL_SHIM_H */
