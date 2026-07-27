/* The terrain chain's END STAGE: pure geometry, no GL. It takes only the HEIGHT FIELD of the ENU
 * mesh osmmesh already stitched, and re-projects EVERY node through the exact Mercator inverse and
 * geodetic->ECEF — so there is no tangent-plane error and no dependence on a fixed home origin.
 * Ausgabevertrag (verts/err/origin_out) und Schuerzen: doc/flightbox/world-and-terrain.md §5.2. */
#ifndef FBCHUNKMESH_H
#define FBCHUNKMESH_H
#include "FBChunkVtx.h" /* w3_vtx, w3_chunk, w3_chunk_free -- no dependency on the ENU builder */
#include <math.h>
#include "geo.h"
#include "mesh.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int w3_chunk_build_ecef(const osmmesh_mesh *m, int z, uint32_t x, uint32_t y, int grid,
                               w3_chunk *out, double origin_out[3]) {
  if (!out || !origin_out) return 0;
  out->verts = 0;
  out->nverts = 0;
  out->err = 0.f;
  origin_out[0] = origin_out[1] = origin_out[2] = 0.0;
  if (!m || !m->positions || m->n_vertices == 0) return 0;

  /* Grid width C, row-major north->south; the same detector both builders use, so a tile that is a
   * regular grid in one is a regular grid in the other. */
  uint32_t C = 0;
  for (uint32_t i = 1; i < m->n_vertices; i++)
    if (m->positions[i * 3] < m->positions[(i - 1) * 3] - 0.5f) {
      C = i;
      break;
    }
  uint32_t R = C ? m->n_vertices / C : 0;
  if (!(C >= 2 && R >= 2 && m->n_vertices % C == 0))
    return 0; /* terrain is always a grid; refuse soup */

  int G = grid < 2 ? 2 : grid;
  int gc = (int)C < G + 1 ? (int)C : G + 1, gr = (int)R < G + 1 ? (int)R : G + 1;
#define W3_MH(r, c) (m->positions[((size_t)(r) * C + (size_t)(c)) * 3 + 2]) /* source height */
#define W3_RI(j) ((int)((long)(j) * (R - 1) / (gr - 1)))
#define W3_CI(i) ((int)((long)(i) * (C - 1) / (gc - 1)))

  int NN = gr * gc;
  double *pe =
      (double *)malloc((size_t)NN * 3 * sizeof(double)); /* node ECEF offset (double, for the normal cross) */
  float *nh = (float *)malloc((size_t)NN * sizeof(float)); /* node source height (for the err walk) */
  if (!pe || !nh) {
    free(pe);
    free(nh);
    return 0;
  }

  /* The tile CENTRE on the ellipsoid, so every offset stays <= half a diagonal plus local relief. */
  {
    int rc = W3_RI(gr / 2), cc = W3_CI(gc / 2);
    osmmesh_geo gc0 = osmmesh_tile_frac_to_geo((uint8_t)z, x, y, 0.5, 0.5);
    gc0.alt = W3_MH(rc, cc);
    osmmesh_ecef o = osmmesh_geo_to_ecef(gc0);
    origin_out[0] = o.x;
    origin_out[1] = o.y;
    origin_out[2] = o.z;
  }

  for (int j = 0; j < gr; j++)
    for (int i = 0; i < gc; i++) {
      int r = W3_RI(j), c = W3_CI(i);
      double fx = (double)c / (double)(C - 1), fy = (double)r / (double)(R - 1);
      float h = W3_MH(r, c);
      osmmesh_geo g = osmmesh_tile_frac_to_geo((uint8_t)z, x, y, fx, fy);
      g.alt = h;
      osmmesh_ecef e = osmmesh_geo_to_ecef(g);
      double *d = pe + ((size_t)j * gc + i) * 3;
      d[0] = e.x - origin_out[0];
      d[1] = e.y - origin_out[1];
      d[2] = e.z - origin_out[2]; /* double subtract */
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
  float *nv = (float *)malloc((size_t)NN * 3 * sizeof(float));
  if (!nv) {
    free(pe);
    free(nh);
    return 0;
  }
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
      float h00 = nh[(size_t)j * gc + i], h10 = nh[(size_t)j * gc + i + 1];
      float h11 = nh[(size_t)(j + 1) * gc + i + 1], h01 = nh[(size_t)(j + 1) * gc + i];
      if (r1 <= r0 || c1 <= c0) continue;
      for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
          float sv = (float)(r - r0) / (float)(r1 - r0), su = (float)(c - c0) / (float)(c1 - c0);
          float h = (su >= sv) ? h00 + (h10 - h00) * su + (h11 - h10) * sv
                               : h00 + (h11 - h01) * su + (h01 - h00) * sv;
          float d = fabsf(h - W3_MH(r, c));
          if (d > err) err = d;
        }
    }
  out->err = err;
  float skirt = 2.f * err;
  if (skirt < 5.f) skirt = 5.f;

  int nquad = (gr - 1) * (gc - 1), nedge = 2 * ((gr - 1) + (gc - 1));
  w3_vtx *v = (w3_vtx *)malloc(((size_t)nquad + nedge) * 6 * sizeof(w3_vtx));
  size_t o = 0;
  if (!v) {
    free(pe);
    free(nh);
    free(nv);
    return 0;
  }
#define W3_UV(j, i)                                                                                \
  (float)(((double)W3_CI(i)) / (double)(C - 1)), (float)(((double)W3_RI(j)) / (double)(R - 1))
  for (int j = 0; j < gr - 1; j++)
    for (int i = 0; i < gc - 1; i++) {
      int qj[6] = {j, j, j + 1, j, j + 1, j + 1}, qi[6] = {i, i + 1, i + 1, i, i + 1, i};
      for (int k = 0; k < 6; k++) {
        const double *P = pe + ((size_t)qj[k] * gc + qi[k]) * 3;
        const float *N = nv + ((size_t)qj[k] * gc + qi[k]) * 3;
        w3_vtx *d = &v[o++];
        d->pos[0] = (float)P[0];
        d->pos[1] = (float)P[1];
        d->pos[2] = (float)P[2];
        d->uv[0] = (float)((double)W3_CI(qi[k]) / (double)(C - 1));
        d->uv[1] = (float)((double)W3_RI(qj[k]) / (double)(R - 1));
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
      w3_vtx *d = &v[o++];                                                                         \
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
#undef W3_UV
#undef W3_MH
#undef W3_RI
#undef W3_CI
  free(pe);
  free(nh);
  free(nv);
  out->verts = v;
  out->nverts = (int)o;
  return 1;
}

#endif /* FBCHUNKMESH_H */
