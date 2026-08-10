/* The shared mesh container: the NODE GRID of a terrain tile, row-major north->south, in ENU metres
 * around the configured origin. The CALLER owns the memory once a generator returns
 * (osmmesh_mesh_free). Topology is not carried: the drawn triangles are cut by ChunkQuadWinding
 * (world/ChunkSurface.h), which is the only place the diagonal is stated. */

#ifndef OSMMESH_MESH_H
#define OSMMESH_MESH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    *positions;   /* 3*n_vertices, ENU meters (e, n, u) */
    uint32_t  n_vertices;
} osmmesh_mesh;

/* Safe on an already-zeroed struct, and safe twice. */
void osmmesh_mesh_free(osmmesh_mesh *m);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_MESH_H */
