/* The terrain vertex LAYOUT: the one contract between the writer (the tile worker's mesh build) and
 * the reader (the draw call). Alone in its own header because the main thread needs only the layout,
 * and including the build functions where nothing calls them is -Wunused-function under -Werror.
 *
 * Stride and offsets are DERIVED from the struct and pinned by static_assert: they used to be literals
 * spelled out at the draw call, and getting one wrong does not error — it renders garbage. */
#ifndef FBCHUNKVTX_H
#define FBCHUNKVTX_H
#include <stddef.h>

typedef struct {
  float pos[3];
  float uv[2];
  float norm[3];
} w3_vtx;

/* Here with the layout because BOTH builders (ENU and ECEF) produce it, and neither should have to
 * include the other just for the struct. */
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
