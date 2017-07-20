#pragma once

#include "../piMesh.h"

namespace piLibs {


int piMeshObj_Read( piMesh *mesh, const wchar_t *name, const piMeshVertexFormat * vf, bool calcNormals );

} // namespace piLibs