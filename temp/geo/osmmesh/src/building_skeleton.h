/* libosmmesh/src/building_skeleton.h
 *
 * Straight skeleton of a simple 2-D polygon with face-per-edge topology.
 * Ports streetwalk's roofs/_skeleton.py + roofs/_polyskel.py (Felkel &
 * Obdrzalek 1998) into C99.
 *
 * The result mirrors CGAL-style skeletons: one closed face per original
 * polygon edge, expressed as a chain of vertex indices. Each vertex
 * carries (x,y) and a "skeleton time" t — the perpendicular distance the
 * eaves have receded inward to reach that vertex. t=0 at eaves, t=t_max
 * at the deepest interior point.
 *
 * Private to libosmmesh — do NOT move under include/osmmesh/.
 */

#ifndef OSMMESH_BUILDING_SKELETON_H
#define OSMMESH_BUILDING_SKELETON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float xy[2];
    float t;         /* "ridge time" — perpendicular recede distance. */
} osmmesh_skeleton_vertex;

typedef struct {
    uint32_t *indices;      /* index into osmmesh_skeleton.vertices. */
    uint32_t  n_indices;    /* vertex count in this face. */
} osmmesh_skeleton_face;

typedef struct {
    osmmesh_skeleton_vertex *vertices;
    uint32_t                 n_vertices;
    osmmesh_skeleton_face   *faces;
    uint32_t                 n_faces;
    float                    t_max;   /* max t across all vertices. */
} osmmesh_skeleton;

/* Compute the straight skeleton of a CCW simple polygon. `xy` is
 * interleaved [x0,y0,...] with `n` vertices (no trailing duplicate).
 *
 * Returns a newly allocated osmmesh_skeleton on success. Caller frees
 * with osmmesh_skeleton_free. Returns NULL on:
 *   - n < 3
 *   - degenerate input (zero-area, self-intersecting, or a configuration
 *     the Felkel event loop can't resolve robustly).
 *
 * The algorithm internally recenters the input at its centroid before
 * running — numerical stability of the event queue depends on it, and
 * the returned vertices are expressed in the original (world) frame. */
osmmesh_skeleton *osmmesh_skeleton_compute(const float *xy, uint32_t n);

/* Release the skeleton and all its owned buffers. Safe on NULL. */
void osmmesh_skeleton_free(osmmesh_skeleton *sk);

#ifdef __cplusplus
}
#endif

#endif /* OSMMESH_BUILDING_SKELETON_H */
