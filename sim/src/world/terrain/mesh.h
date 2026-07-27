/* The shared mesh container: plain SoA, flat float arrays plus flat uint32 indices, in ENU metres
 * around the configured origin. The CALLER owns the memory once a generator returns
 * (osmmesh_mesh_free). Normals are unit length; normals and UVs are both optional. */

#ifndef OSMMESH_MESH_H
#define OSMMESH_MESH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    *positions;   /* 3*n_vertices, ENU meters (e, n, u) */
    float    *normals;     /* 3*n_vertices, unit length; NULL allowed */
    float    *uvs;         /* 2*n_vertices; NULL allowed */
    uint32_t *indices;     /* 3*n_triangles */
    uint32_t  n_vertices;
    uint32_t  n_triangles;
} osmmesh_mesh;

/* Safe on an already-zeroed struct, and safe twice. */
void osmmesh_mesh_free(osmmesh_mesh *m);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_MESH_H */
