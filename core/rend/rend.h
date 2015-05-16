#pragma once
#include "hw/pvr/ta_ctx.h"
#include "hw/pvr/Renderer_if.h"


#ifdef GLuint
GLuint
#else
u32
#endif
GetTexture(TSP tsp,TCW tcw);
