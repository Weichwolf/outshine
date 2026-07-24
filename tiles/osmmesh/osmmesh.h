/* tiles/osmmesh/osmmesh.h
 *
 * Server-side subset of the osmmesh meta-header (T8 split, 2026-07-24): fb-tiles rasterises MVT
 * vector tiles directly (raster.c/lights.c) and decodes Terrarium DEM PNGs for point-elevation
 * queries (elev.c) -- it never builds meshes and never opens PMTiles archives (it fetches raw
 * bytes over HTTP itself, see cache.c), so this pulls in only mvt.h + terrain.h. The client's
 * full osmmesh (ctx/fetch_tile/config, building/linear mesh generation, ECEF terrain meshing,
 * PMTiles archive IO) is a separate, independently-vendored copy -- see temp/geo/osmmesh (Teil 2
 * migrates it to sim/ as C++). Both copies share mvt.c/pb_stream.c and terrain.c's decode/grid-
 * free pair verbatim (kept in sync manually, like the vendored stb headers) because each side
 * needs its own independent build, not because the code differs.
 */

#ifndef OSMMESH_H
#define OSMMESH_H

#include "osmmesh/mvt.h"
#include "osmmesh/terrain.h"

#endif /* OSMMESH_H */
