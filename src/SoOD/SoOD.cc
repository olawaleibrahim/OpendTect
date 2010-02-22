/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : K. Tingdahl
 * DATE     : Oct 1999
-*/

static const char* rcsID = "$Id: SoOD.cc,v 1.24 2010-02-22 09:56:28 cvsbruno Exp $";


#include "SoOD.h"

#include "Inventor/nodes/SoFragmentShader.h"
#include "Inventor/nodes/SoVertexShader.h"
#include "Inventor/nodes/SoTextureUnit.h"

#ifdef lux
extern "C" { extern void* glXGetCurrentContext(); }
#elif mac
extern "C" { extern void* coin_gl_current_context(); }
#endif

int SoOD::supportsFragShading()
{
    static int answer = 0;
    if ( !answer )
    {
#ifdef win
	if ( wglGetCurrentContext() )
#elif lux
	if ( glXGetCurrentContext() )
#else
	if ( coin_gl_current_context() )
#endif
	    answer =
		SoFragmentShader::isSupported( SoShaderObject::GLSL_PROGRAM)
		    ? 1 : -1;
    }

    return answer;
}


int SoOD::supportsVertexShading()
{
    static int answer = 0;
    if ( !answer )
    {
#ifdef win
	if ( wglGetCurrentContext() )
#elif lux
	if ( glXGetCurrentContext() )
#else
	if ( coin_gl_current_context() )
#endif
	    answer = SoVertexShader::isSupported( SoShaderObject::GLSL_PROGRAM )
		    ? 1 : -1;
    }

    return answer;
}


int SoOD::maxNrTextureUnits()
{
    static int answer = 0;
    if ( !answer )
    {
#ifdef win
      if ( wglGetCurrentContext() )
#elif lux
      if ( glXGetCurrentContext() )
#else
      if ( coin_gl_current_context() )
#endif
          answer = SoTextureUnit::getMaxTextureUnit ();
    }

    return answer ? answer : 1;
}


void SoOD::getLineWidthBounds( int& min, int& max )
{
    GLfloat bounds[2];
    glGetFloatv( GL_LINE_WIDTH_RANGE, bounds );
    min = (int)bounds[0];
    max = (int)bounds[1];
}

