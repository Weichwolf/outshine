#ifndef OUTSHINE_WORLD_PRODUCTS_SURFACETILESTITCH_H
#define OUTSHINE_WORLD_PRODUCTS_SURFACETILESTITCH_H

#include "SurfaceTile.h"
#include <span>

namespace outshine {

void StitchSurfaceTiles(std::span<SurfaceTile> tiles);

}
#endif
