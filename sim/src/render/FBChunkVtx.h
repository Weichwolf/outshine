/* FlightBox renderer — the terrain vertex LAYOUT: the one contract shared by the two sides.
 *
 * The WRITER is the tile worker (FBChunkMesh.h -> w3_chunk_build_ecef, off the main thread). The
 * READER is the draw call on the main thread (glVertexAttribPointer, via W3_VTX_STRIDE/W3_VTX_OFF
 * in world3d.h). They used to live in one header, but since the mesh build moved to the worker the
 * main thread needs ONLY this layout, not the build maths -- and including the build functions
 * where nothing calls them is a -Wunused-function error under -Werror. So the layout that both
 * sides must agree on lives here, alone; the build that only the worker runs stays in
 * FBChunkMesh.h.
 *
 * The writer and the reader used to agree only by hand: literal stride 32 and offsets 0/12/20
 * spelled out at the draw call. When normals were added the stride went 20 -> 32 and every one of
 * those numbers had to change together; getting one wrong does not error, it renders garbage.
 * Derive them from the struct instead, and pin the result so a layout change breaks the build
 * rather than the picture.
 */
#ifndef FBCHUNKVTX_H
#define FBCHUNKVTX_H
#include <stddef.h>

typedef struct {
  float pos[3];
  float uv[2];
  float norm[3];
} w3_vtx;

/* One built terrain chunk: a malloc'd w3_vtx array + its measured geometric error. Lives here, with
 * the vertex layout, because BOTH builders produce it -- chunkmesh.h (ENU) and chunkmesh_ecef.h
 * (global ECEF) -- and the ECEF builder must not have to include the ENU one just for the struct.
 */
#include <stdlib.h>
typedef struct {
  w3_vtx *verts;
  int nverts;
  float err; /* max |drawn surface - source height| in METRES; drives the LOD.
              * 0 for the irregular-mesh fallback: nothing was decimated there. */
} w3_chunk;
static inline void w3_chunk_free(w3_chunk *c) {
  if (!c) return;
  free(c->verts);
  c->verts = 0;
  c->nverts = 0;
  c->err = 0.f;
}
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(w3_vtx) == 8 * sizeof(float),
               "terrain vertex must be tightly packed (no padding)");
_Static_assert(offsetof(w3_vtx, pos) == 0, "aPos offset");
_Static_assert(offsetof(w3_vtx, uv) == 12, "aUV offset");
_Static_assert(offsetof(w3_vtx, norm) == 20, "aNorm offset");
#endif

#endif /* FBCHUNKVTX_H */
