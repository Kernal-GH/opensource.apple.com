/*
# AUTOGENERATED DO NOT EDIT

# If you edit this file, delete the AUTOGENERATED line to prevent re-generation
# BUILD api_versions [0x001]
*/

%module fragment_lighting

#define __version__ "$Revision: 1.1.2.1 $"
#define __date__ "$Date: 2004/11/15 07:38:07 $"
#define __api_version__ API_VERSION
#define __author__ "PyOpenGL Developers <pyopengl-devel@lists.sourceforge.net>"
#define __doc__ ""

%{
/**
 *
 * GL.SGIX.fragment_lighting Module for PyOpenGL
 * 
 * Authors: PyOpenGL Developers <pyopengl-devel@lists.sourceforge.net>
 * 
***/
%}

%include util.inc
%include gl_exception_handler.inc

%{
#ifdef CGL_PLATFORM
# include <OpenGL/glext.h>
#else
# include <GL/glext.h>
#endif

#if !EXT_DEFINES_PROTO || !defined(GL_SGIX_fragment_lighting)
DECLARE_VOID_EXT(glFragmentColorMaterialSGIX, (GLenum face, GLenum mode), (face, mode))
DECLARE_VOID_EXT(glFragmentLightfSGIX, (GLenum light, GLenum pname, GLfloat param), (light, pname, param))
DECLARE_VOID_EXT(glFragmentLightfvSGIX, (GLenum light, GLenum pname, const GLfloat *params), (light, pname, params))
DECLARE_VOID_EXT(glFragmentLightiSGIX, (GLenum light, GLenum pname, GLint param), (light, pname, param))
DECLARE_VOID_EXT(glFragmentLightivSGIX, (GLenum light, GLenum pname, const GLint *params), (light, pname, params))
DECLARE_VOID_EXT(glFragmentLightModelfSGIX, (GLenum pname, GLfloat param), (pname, param))
DECLARE_VOID_EXT(glFragmentLightModelfvSGIX, (GLenum pname, const GLfloat *params), (pname, params))
DECLARE_VOID_EXT(glFragmentLightModeliSGIX, (GLenum pname, GLint param), (pname, param))
DECLARE_VOID_EXT(glFragmentLightModelivSGIX, (GLenum pname, const GLint *params), (pname, params))
DECLARE_VOID_EXT(glFragmentMaterialfSGIX, (GLenum face, GLenum pname, GLfloat param), (face, pname, param))
DECLARE_VOID_EXT(glFragmentMaterialfvSGIX, (GLenum face, GLenum pname, const GLfloat *params), (face, pname, params))
DECLARE_VOID_EXT(glFragmentMaterialiSGIX, (GLenum face, GLenum pname, GLint param), (face, pname, param))
DECLARE_VOID_EXT(glFragmentMaterialivSGIX, (GLenum face, GLenum pname, const GLint *params), (face, pname, params))
DECLARE_VOID_EXT(glGetFragmentLightfvSGIX, (GLenum light, GLenum pname, GLfloat *params), (light, pname, params))
DECLARE_VOID_EXT(glGetFragmentLightivSGIX, (GLenum light, GLenum pname, GLint *params), (light, pname, params))
DECLARE_VOID_EXT(glGetFragmentMaterialfvSGIX, (GLenum face, GLenum pname, GLfloat *params), (face, pname, params))
DECLARE_VOID_EXT(glGetFragmentMaterialivSGIX, (GLenum face, GLenum pname, GLint *params), (face, pname, params))
DECLARE_VOID_EXT(glLightEnviSGIX, (GLenum pname, GLint param), (pname, param))
#endif
%}

/* FUNCTION DECLARATIONS */
void glFragmentColorMaterialSGIX(GLenum face, GLenum mode);
DOC(glFragmentColorMaterialSGIX, "glFragmentColorMaterialSGIX(face, mode)")
void glFragmentLightfSGIX(GLenum light, GLenum pname, GLfloat param);
DOC(glFragmentLightfSGIX, "glFragmentLightfSGIX(light, pname, param)")
void glFragmentLightfvSGIX(GLenum light, GLenum pname, const GLfloat *params);
DOC(glFragmentLightfvSGIX, "glFragmentLightfvSGIX(light, pname, params)")
void glFragmentLightiSGIX(GLenum light, GLenum pname, GLint param);
DOC(glFragmentLightiSGIX, "glFragmentLightiSGIX(light, pname, param)")
void glFragmentLightivSGIX(GLenum light, GLenum pname, const GLint *params);
DOC(glFragmentLightivSGIX, "glFragmentLightivSGIX(light, pname, params)")
void glFragmentLightModelfSGIX(GLenum pname, GLfloat param);
DOC(glFragmentLightModelfSGIX, "glFragmentLightModelfSGIX(pname, param)")
void glFragmentLightModelfvSGIX(GLenum pname, const GLfloat *params);
DOC(glFragmentLightModelfvSGIX, "glFragmentLightModelfvSGIX(pname, params)")
void glFragmentLightModeliSGIX(GLenum pname, GLint param);
DOC(glFragmentLightModeliSGIX, "glFragmentLightModeliSGIX(pname, param)")
void glFragmentLightModelivSGIX(GLenum pname, const GLint *params);
DOC(glFragmentLightModelivSGIX, "glFragmentLightModelivSGIX(pname, params)")
void glFragmentMaterialfSGIX(GLenum face, GLenum pname, GLfloat param);
DOC(glFragmentMaterialfSGIX, "glFragmentMaterialfSGIX(face, pname, param)")
void glFragmentMaterialfvSGIX(GLenum face, GLenum pname, const GLfloat *params);
DOC(glFragmentMaterialfvSGIX, "glFragmentMaterialfvSGIX(face, pname, params)")
void glFragmentMaterialiSGIX(GLenum face, GLenum pname, GLint param);
DOC(glFragmentMaterialiSGIX, "glFragmentMaterialiSGIX(face, pname, param)")
void glFragmentMaterialivSGIX(GLenum face, GLenum pname, const GLint *params);
DOC(glFragmentMaterialivSGIX, "glFragmentMaterialivSGIX(face, pname, params)")
void glGetFragmentLightfvSGIX(GLenum light, GLenum pname, GLfloat *params);
DOC(glGetFragmentLightfvSGIX, "glGetFragmentLightfvSGIX(light, pname, params)")
void glGetFragmentLightivSGIX(GLenum light, GLenum pname, GLint *params);
DOC(glGetFragmentLightivSGIX, "glGetFragmentLightivSGIX(light, pname, params)")
void glGetFragmentMaterialfvSGIX(GLenum face, GLenum pname, GLfloat *params);
DOC(glGetFragmentMaterialfvSGIX, "glGetFragmentMaterialfvSGIX(face, pname, params)")
void glGetFragmentMaterialivSGIX(GLenum face, GLenum pname, GLint *params);
DOC(glGetFragmentMaterialivSGIX, "glGetFragmentMaterialivSGIX(face, pname, params)")
void glLightEnviSGIX(GLenum pname, GLint param);
DOC(glLightEnviSGIX, "glLightEnviSGIX(pname, param)")

/* CONSTANT DECLARATIONS */
#define      GL_FRAGMENT_LIGHTING_SGIX 0x8400
#define GL_FRAGMENT_COLOR_MATERIAL_SGIX 0x8401
#define GL_FRAGMENT_COLOR_MATERIAL_FACE_SGIX 0x8402
#define GL_FRAGMENT_COLOR_MATERIAL_PARAMETER_SGIX 0x8403
#define    GL_MAX_FRAGMENT_LIGHTS_SGIX 0x8404
#define      GL_MAX_ACTIVE_LIGHTS_SGIX 0x8405
#define  GL_CURRENT_RASTER_NORMAL_SGIX 0x8406
#define         GL_LIGHT_ENV_MODE_SGIX 0x8407
#define GL_FRAGMENT_LIGHT_MODEL_LOCAL_VIEWER_SGIX 0x8408
#define GL_FRAGMENT_LIGHT_MODEL_TWO_SIDE_SGIX 0x8409
#define GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX 0x840A
#define GL_FRAGMENT_LIGHT_MODEL_NORMAL_INTERPOLATION_SGIX 0x840B
#define        GL_FRAGMENT_LIGHT0_SGIX 0x840C
#define        GL_FRAGMENT_LIGHT1_SGIX 0x840D
#define        GL_FRAGMENT_LIGHT2_SGIX 0x840E
#define        GL_FRAGMENT_LIGHT3_SGIX 0x840F
#define        GL_FRAGMENT_LIGHT4_SGIX 0x8410
#define        GL_FRAGMENT_LIGHT5_SGIX 0x8411
#define        GL_FRAGMENT_LIGHT6_SGIX 0x8412
#define        GL_FRAGMENT_LIGHT7_SGIX 0x8413


%{
static char *proc_names[] =
{
#if !EXT_DEFINES_PROTO || !defined(GL_SGIX_fragment_lighting)
"glFragmentColorMaterialSGIX",
"glFragmentLightfSGIX",
"glFragmentLightfvSGIX",
"glFragmentLightiSGIX",
"glFragmentLightivSGIX",
"glFragmentLightModelfSGIX",
"glFragmentLightModelfvSGIX",
"glFragmentLightModeliSGIX",
"glFragmentLightModelivSGIX",
"glFragmentMaterialfSGIX",
"glFragmentMaterialfvSGIX",
"glFragmentMaterialiSGIX",
"glFragmentMaterialivSGIX",
"glGetFragmentLightfvSGIX",
"glGetFragmentLightivSGIX",
"glGetFragmentMaterialfvSGIX",
"glGetFragmentMaterialivSGIX",
"glLightEnviSGIX",
#endif
	NULL
};

#define glInitFragmentLightingSGIX() InitExtension("GL_SGIX_fragment_lighting", proc_names)
%}

int glInitFragmentLightingSGIX();
DOC(glInitFragmentLightingSGIX, "glInitFragmentLightingSGIX() -> bool")

%{
PyObject *__info()
{
	if (glInitFragmentLightingSGIX())
	{
		PyObject *info = PyList_New(0);
		return info;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}
%}

PyObject *__info();

