/* libosmmesh/include/osmmesh/mesh.h
 *
 * Shared mesh container used by all mesh generators (T5 terrain,
 * T6 buildings, T7 linear features). Plain SoA: positions/normals/uvs
 * as flat float arrays, indices as flat uint32. Caller owns the memory
 * once the generator returns; `osmmesh_mesh_free` releases it.
 *
 * All coordinates are ENU meters (east, north, up) around the configured
 * origin from `osmmesh_enu_ctx`. Normals are unit length. UVs are optional;
 * normals are optional.
 */

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

/* Free every non-NULL data pointer inside `m` and zero the struct. Safe
 * on an already-zeroed struct; safe to call twice. */
void osmmesh_mesh_free(osmmesh_mesh *m);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_MESH_H */
