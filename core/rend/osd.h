#include <vector>
#include "types.h"

#define VJOY_VISIBLE 14
#define OSD_TEX_W 512
#define OSD_TEX_H 256

struct OSDVertex
{
	float x, y;
	float u, v;
	u8 r, g, b, a;
};

const std::vector<OSDVertex>& GetOSDVertices();
