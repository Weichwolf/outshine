/* The terrain chain's END STAGE: pure geometry, no GL. It takes only the HEIGHT FIELD of the ENU
 * mesh TerrainTiles already stitched, and re-projects EVERY node through the exact Mercator inverse and
 * geodetic->ECEF — so there is no tangent-plane error and no dependence on a fixed home origin. */
#ifndef CHUNKMESH_H
#define CHUNKMESH_H
#include "ChunkSurface.h" /* the node lattice and the split, shared with the height oracle */
#include "ChunkVtx.h" /* ChunkVtx, Chunk, ChunkFree -- no dependency on the ENU builder */
#include "Heap.h"
#include <math.h>
#include "TerrainGrid.h"
#include "TileGeodesy.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace outshine::World {

inline int ChunkBuildEcef(const TerrainMesh &mesh, int z, uint32_t x, uint32_t y, int grid,
                           Chunk *out, double origin_out[3]) {
  if (!out || !origin_out) return 0;
  out->verts = 0;
  out->nverts = 0;
  out->gridverts = 0;
  out->err = 0.f;
  origin_out[0] = origin_out[1] = origin_out[2] = 0.0;
  const std::vector<float> *positions = mesh.TryPositionsEnuM();
  if (!positions || positions->empty()) return 0;
  const float *p = positions->data();
  const uint32_t nVertices = mesh.VertexCount();

  /* Grid width C, row-major north->south; the same detector both builders use, so a tile that is a
   * regular grid in one is a regular grid in the other. */
  uint32_t C = 0;
  for (uint32_t i = 1; i < nVertices; i++)
    if (p[i * 3] < p[(i - 1) * 3] - 0.5f) {
      C = i;
      break;
    }
  uint32_t R = C ? nVertices / C : 0;
  if (!(C >= 2 && R >= 2 && nVertices % C == 0))
    return 0; /* terrain is always a grid; refuse soup */

  int gc = ChunkNodes(C, grid), gr = ChunkNodes(R, grid);
#define W3_MH(r, c) (p[((size_t)(r) * C + (size_t)(c)) * 3 + 2]) /* source height */
#define W3_RI(j) ((int)ChunkNodePosting((j), R, gr))
#define W3_CI(i) ((int)ChunkNodePosting((i), C, gc))

  int NN = gr * gc;
  /* THE ONLY ANSWER THIS FUNCTION HAS IS "no mesh", and its caller reads that as a hole in the world
   * (world/TilePool.cpp RunMesh). An exhausted heap is not a hole, so it ends the run here where the
   * item and the byte count are still known, instead of arriving as a quadrant that never comes. */
  double *pe = (double *)Heap::Take("terrain node offsets", (size_t)NN * 3 * sizeof(double));
  float *nh = (float *)Heap::Take("terrain node heights", (size_t)NN * sizeof(float));

  /* The tile CENTRE on the ellipsoid, so every offset stays <= half a diagonal plus local relief. */
  {
    int rc = W3_RI(gr / 2), cc = W3_CI(gc / 2);
    Geo gc0 = TileFracToGeo(z, x, y, 0.5, 0.5);
    gc0.AltM = W3_MH(rc, cc);
    Ecef o = GeoToEcefWgs84(gc0);
    origin_out[0] = o.X;
    origin_out[1] = o.Y;
    origin_out[2] = o.Z;
  }

  for (int j = 0; j < gr; j++)
    for (int i = 0; i < gc; i++) {
      int r = W3_RI(j), c = W3_CI(i);
      double fx = (double)c / (double)(C - 1), fy = (double)r / (double)(R - 1);
      float h = W3_MH(r, c);
      Geo g = TileFracToGeo(z, x, y, fx, fy);
      g.AltM = h;
      Ecef e = GeoToEcefWgs84(g);
      double *d = pe + ((size_t)j * gc + i) * 3;
      d[0] = e.X - origin_out[0];
      d[1] = e.Y - origin_out[1];
      d[2] = e.Z - origin_out[2]; /* double subtract */
      nh[(size_t)j * gc + i] = h;
    }

  /* The origin is on the ellipsoid, so geodetic up ~ normalize(origin) to well under a degree. */
  double olen = sqrt(origin_out[0] * origin_out[0] + origin_out[1] * origin_out[1] +
                     origin_out[2] * origin_out[2]);
  if (olen < 1.0) olen = 1.0;
  float radial[3] = {(float)(origin_out[0] / olen), (float)(origin_out[1] / olen),
                     (float)(origin_out[2] / olen)};

  /* Central differences over the DECIMATED neighbours, so the light matches the drawn silhouette.
   * Cross products are translation-invariant, so the small float offsets give the TRUE geometric
   * normal — the tile's curvature comes along for free. */
  float *nv = (float *)Heap::Take("terrain node normals", (size_t)NN * 3 * sizeof(float));
  for (int j = 0; j < gr; j++)
    for (int i = 0; i < gc; i++) {
      int i0 = i > 0 ? i - 1 : i, i1 = i < gc - 1 ? i + 1 : i, j0 = j > 0 ? j - 1 : j,
          j1 = j < gr - 1 ? j + 1 : j;
      const double *W = pe + ((size_t)j * gc + i0) * 3, *E = pe + ((size_t)j * gc + i1) * 3;
      const double *Nn = pe + ((size_t)j0 * gc + i) * 3, *Sn = pe + ((size_t)j1 * gc + i) * 3;
      double te[3] = {E[0] - W[0], E[1] - W[1], E[2] - W[2]}; /* east tangent  */
      double tn[3] = {Nn[0] - Sn[0], Nn[1] - Sn[1],
                      Nn[2] - Sn[2]}; /* north tangent (row j-1 is north)     */
      double nx = te[1] * tn[2] - te[2] * tn[1], ny = te[2] * tn[0] - te[0] * tn[2],
             nz = te[0] * tn[1] - te[1] * tn[0];
      double L = sqrt(nx * nx + ny * ny + nz * nz);
      if (L < 1e-9) {
        nx = radial[0];
        ny = radial[1];
        nz = radial[2];
        L = 1;
      }
      float fx = (float)(nx / L), fy = (float)(ny / L), fz = (float)(nz / L);
      if (fx * radial[0] + fy * radial[1] + fz * radial[2] < 0) {
        fx = -fx;
        fy = -fy;
        fz = -fz;
      } /* keep outward */
      float *o = nv + ((size_t)j * gc + i) * 3;
      o[0] = fx;
      o[1] = fy;
      o[2] = fz;
    }

  /* Walk every SOURCE pixel, evaluate the drawn surface there, keep the worst vertical miss. A
   * height-field property, so the ECEF projection cannot change it. */
  float err = 0.f;
  for (int j = 0; j < gr - 1; j++)
    for (int i = 0; i < gc - 1; i++) {
      int r0 = W3_RI(j), r1 = W3_RI(j + 1), c0 = W3_CI(i), c1 = W3_CI(i + 1);
      if (r1 <= r0 || c1 <= c0) continue;
      const ChunkCell cell{nh, gc, j, i};
      for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
          float sv = (float)(r - r0) / (float)(r1 - r0), su = (float)(c - c0) / (float)(c1 - c0);
          float d = fabsf(ChunkCellHeight(cell, su, sv) - W3_MH(r, c));
          if (d > err) err = d;
        }
    }
  out->err = err;
  float skirt = 2.f * err;
  if (skirt < 5.f) skirt = 5.f;

  int nquad = (gr - 1) * (gc - 1), nedge = 2 * ((gr - 1) + (gc - 1));
  ChunkVtx *v =
      (ChunkVtx *)Heap::Take("terrain tile vertices", ((size_t)nquad + nedge) * 6 * sizeof(ChunkVtx));
  size_t o = 0;
  for (int j = 0; j < gr - 1; j++)
    for (int i = 0; i < gc - 1; i++) {
      for (const ChunkQuadCorner &corner : ChunkQuadWinding()) {
        const int qj = j + corner.Row, qi = i + corner.Col;
        const double *P = pe + ((size_t)qj * gc + qi) * 3;
        const float *N = nv + ((size_t)qj * gc + qi) * 3;
        ChunkVtx *d = &v[o++];
        d->pos[0] = (float)P[0];
        d->pos[1] = (float)P[1];
        d->pos[2] = (float)P[2];
        d->uv[0] = (float)((double)W3_CI(qi) / (double)(C - 1));
        d->uv[1] = (float)((double)W3_RI(qj) / (double)(R - 1));
        d->norm[0] = N[0];
        d->norm[1] = N[1];
        d->norm[2] = N[2];
      }
    }
/* A curtain hanging along -radial from each boundary edge, textured with that edge's own texels:
 * it bounds the LOD-seam crack. */
#define W3_SKIRT_EDGE(aj, ai, bj, bi)                                                              \
  do {                                                                                             \
    const double *A = pe + ((size_t)(aj) * gc + (ai)) * 3,                                         \
                 *B = pe + ((size_t)(bj) * gc + (bi)) * 3;                                         \
    float au = (float)((double)W3_CI(ai) / (double)(C - 1)),                                       \
          av = (float)((double)W3_RI(aj) / (double)(R - 1));                                       \
    float bu = (float)((double)W3_CI(bi) / (double)(C - 1)),                                       \
          bv = (float)((double)W3_RI(bj) / (double)(R - 1));                                       \
    float Ax = (float)A[0], Ay = (float)A[1], Az = (float)A[2], Bx = (float)B[0],                  \
          By = (float)B[1], Bz = (float)B[2];                                                      \
    float Adx = Ax - radial[0] * skirt, Ady = Ay - radial[1] * skirt,                              \
          Adz = Az - radial[2] * skirt;                                                            \
    float Bdx = Bx - radial[0] * skirt, Bdy = By - radial[1] * skirt,                              \
          Bdz = Bz - radial[2] * skirt;                                                            \
    float P6[6][3] = {{Ax, Ay, Az}, {Bx, By, Bz},    {Bdx, Bdy, Bdz},                              \
                      {Ax, Ay, Az}, {Bdx, Bdy, Bdz}, {Adx, Ady, Adz}};                             \
    float U6[6][2] = {{au, av}, {bu, bv}, {bu, bv}, {au, av}, {bu, bv}, {au, av}};                 \
    for (int k = 0; k < 6; k++) {                                                                  \
      ChunkVtx *d = &v[o++];                                                                         \
      d->pos[0] = P6[k][0];                                                                        \
      d->pos[1] = P6[k][1];                                                                        \
      d->pos[2] = P6[k][2];                                                                        \
      d->uv[0] = U6[k][0];                                                                         \
      d->uv[1] = U6[k][1];                                                                         \
      d->norm[0] = radial[0];                                                                      \
      d->norm[1] = radial[1];                                                                      \
      d->norm[2] = radial[2];                                                                      \
    }                                                                                              \
  } while (0)
  for (int i = 0; i < gc - 1; i++) {
    W3_SKIRT_EDGE(0, i + 1, 0, i);
  } /* north */
  for (int i = 0; i < gc - 1; i++) {
    W3_SKIRT_EDGE(gr - 1, i, gr - 1, i + 1);
  } /* south */
  for (int j = 0; j < gr - 1; j++) {
    W3_SKIRT_EDGE(j, 0, j + 1, 0);
  } /* west  */
  for (int j = 0; j < gr - 1; j++) {
    W3_SKIRT_EDGE(j + 1, gc - 1, j, gc - 1);
  } /* east  */
#undef W3_SKIRT_EDGE
#undef W3_MH
#undef W3_RI
#undef W3_CI
  free(pe);
  free(nh);
  free(nv);
  out->verts = v;
  out->nverts = (int)o;
  out->gridverts = nquad * 6;
  return 1;
}

} // namespace outshine::World
#endif /* CHUNKMESH_H */
