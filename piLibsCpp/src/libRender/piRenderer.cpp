#include <stdlib.h>
#include "piRenderer.h"

#include "opengl4x/piGL4X_Renderer.h"
#include "directx11/piDX11_Renderer.h"

namespace piLibs {


piRenderer *piRenderer::Create( const piRendererType type )
{
	if( type==GL ) return new piRendererGL4X();
	if( type==DX ) return new piRendererDX11();

	return NULL;
}


piRenderer::piRenderer()
{
}

piRenderer::~piRenderer()
{
}


}