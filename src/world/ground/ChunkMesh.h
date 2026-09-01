#ifndef OUTSHINE_WORLD_GROUND_CHUNKMESH_H
#define OUTSHINE_WORLD_GROUND_CHUNKMESH_H
#include "Vec3.h"
#include "ChunkSurface.h"
#include "ChunkVtx.h"
#include "Heap.h"
#include <math.h>
#include "TerrainGrid.h"
#include "TileGeodesy.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

namespace outshine::Ground {

inline int ChunkBuildEcef(const TerrainMesh &mesh,
                          int z,
                          uint32_t x,
                          uint32_t y,
                          int grid,
                          Chunk *out,
                          Vec3 &origin_out) {
  if (out == nullptr) { return 0; }
  out->verts = 0;
  out->nverts = 0;
  out->gridverts = 0;
  out->err = 0.f;
  origin_out[0] = origin_out[1] = origin_out[2] = 0.0;
  const std::vector<float> *positions = mesh.TryPositionsEnuM();
  if ((positions == nullptr) || positions->empty()) { return 0; }
  const float *p = positions->data();
  const uint32_t nVertices = mesh.VertexCount();

  uint32_t C = 0;
  for (uint32_t i = 1; i < nVertices; i++) {
    if (p[i * 3] < p[(i - 1) * 3] - 0.5f) {
      C = i;
      break;
    }
  }
  const uint32_t R = (C != 0u) ? nVertices / C : 0;
  if (!(C >= 2 && R >= 2 && nVertices % C == 0)) { return 0; }

  int gc = ChunkNodes(C, grid), gr = ChunkNodes(R, grid);
#define W3_MH(r, c) (p[((size_t)(r) * C + (size_t)(c)) * 3 + 2])
#define W3_RI(j) ((int)ChunkNodePosting((j), R, gr))
#define W3_CI(i) ((int)ChunkNodePosting((i), C, gc))

  const int NN = gr * gc;

  auto *pe = static_cast<double *>(
      Heap::Take("terrain node offsets", static_cast<size_t>(NN) * 3 * sizeof(double)));
  auto *nh = static_cast<float *>(
      Heap::Take("terrain node heights", static_cast<size_t>(NN) * sizeof(float)));

  {
    const int rc = W3_RI(gr / 2);
    const int cc = W3_CI(gc / 2);
    Geo gc0 = TileFracToGeo(z, x, y, 0.5, 0.5);
    gc0.AltM = W3_MH(rc, cc);
    const Ecef o = GeoToEcefWgs84(gc0);
    origin_out[0] = o.X;
    origin_out[1] = o.Y;
    origin_out[2] = o.Z;
  }

  for (int j = 0; j < gr; j++) {
    for (int i = 0; i < gc; i++) {
      const int r = W3_RI(j);
      const int c = W3_CI(i);
      const double fx = static_cast<double>(c) / static_cast<double>(C - 1);
      const double fy = static_cast<double>(r) / static_cast<double>(R - 1);
      const float h = W3_MH(r, c);
      Geo g = TileFracToGeo(z, x, y, fx, fy);
      g.AltM = h;
      const Ecef e = GeoToEcefWgs84(g);
      double *d = pe + (static_cast<size_t>(j) * gc + i) * 3;
      d[0] = e.X - origin_out[0];
      d[1] = e.Y - origin_out[1];
      d[2] = e.Z - origin_out[2];
      nh[static_cast<size_t>(j) * gc + i] = h;
    }
  }

  double olen = sqrt(origin_out[0] * origin_out[0] + origin_out[1] * origin_out[1] +
                     origin_out[2] * origin_out[2]);
  olen = std::max(olen, 1.0);
  const Vec3f radial = {{static_cast<float>(origin_out[0] / olen),
                         static_cast<float>(origin_out[1] / olen),
                         static_cast<float>(origin_out[2] / olen)}};

  auto *nv = static_cast<float *>(
      Heap::Take("terrain node normals", static_cast<size_t>(NN) * 3 * sizeof(float)));
  for (int j = 0; j < gr; j++) {
    for (int i = 0; i < gc; i++) {
      const int i0 = i > 0 ? i - 1 : i;
      const int i1 = i < gc - 1 ? i + 1 : i;
      const int j0 = j > 0 ? j - 1 : j;
      const int j1 = j < gr - 1 ? j + 1 : j;
      const double *W = pe + (static_cast<size_t>(j) * gc + i0) * 3;
      const double *E = pe + (static_cast<size_t>(j) * gc + i1) * 3;
      const double *Nn = pe + (static_cast<size_t>(j0) * gc + i) * 3;
      const double *Sn = pe + (static_cast<size_t>(j1) * gc + i) * 3;
      const Vec3 te = {{E[0] - W[0], E[1] - W[1], E[2] - W[2]}};
      const Vec3 tn = {{Nn[0] - Sn[0], Nn[1] - Sn[1], Nn[2] - Sn[2]}};
      double nx = te[1] * tn[2] - te[2] * tn[1];
      double ny = te[2] * tn[0] - te[0] * tn[2];
      double nz = te[0] * tn[1] - te[1] * tn[0];
      double L = sqrt(nx * nx + ny * ny + nz * nz);
      if (L < 1e-9) {
        nx = radial[0];
        ny = radial[1];
        nz = radial[2];
        L = 1;
      }
      auto fx = static_cast<float>(nx / L);
      auto fy = static_cast<float>(ny / L);
      auto fz = static_cast<float>(nz / L);
      if (fx * radial[0] + fy * radial[1] + fz * radial[2] < 0) {
        fx = -fx;
        fy = -fy;
        fz = -fz;
      }
      float *o = nv + (static_cast<size_t>(j) * gc + i) * 3;
      o[0] = fx;
      o[1] = fy;
      o[2] = fz;
    }
  }

  float err = 0.f;
  for (int j = 0; j < gr - 1; j++) {
    for (int i = 0; i < gc - 1; i++) {
      const int r0 = W3_RI(j);
      const int r1 = W3_RI(j + 1);
      const int c0 = W3_CI(i);
      const int c1 = W3_CI(i + 1);
      if (r1 <= r0 || c1 <= c0) { continue; }
      const ChunkCell cell{.Nodes = nh, .Stride = gc, .Row = j, .Col = i};
      for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
          const float sv = static_cast<float>(r - r0) / static_cast<float>(r1 - r0);
          const float su = static_cast<float>(c - c0) / static_cast<float>(c1 - c0);
          const float d = fabsf(ChunkCellHeight(cell, su, sv) - W3_MH(r, c));
          err = std::max(d, err);
        }
      }
    }
  }
  out->err = err;

  const int nquad = (gr - 1) * (gc - 1);
  auto *v = static_cast<ChunkVtx *>(
      Heap::Take("terrain tile vertices", static_cast<size_t>(nquad) * 6 * sizeof(ChunkVtx)));
  size_t o = 0;
  for (int j = 0; j < gr - 1; j++) {
    for (int i = 0; i < gc - 1; i++) {
      for (const ChunkQuadCorner &corner : ChunkQuadWinding()) {
        const int qj = j + corner.Row;
        const int qi = i + corner.Col;
        const double *P = pe + (static_cast<size_t>(qj) * gc + qi) * 3;
        const float *N = nv + (static_cast<size_t>(qj) * gc + qi) * 3;
        ChunkVtx *d = &v[o++];
        d->pos[0] = static_cast<float>(P[0]);
        d->pos[1] = static_cast<float>(P[1]);
        d->pos[2] = static_cast<float>(P[2]);
        d->uv[0] = static_cast<float>(static_cast<double> W3_CI(qi) / static_cast<double>(C - 1));
        d->uv[1] = static_cast<float>(static_cast<double> W3_RI(qj) / static_cast<double>(R - 1));
        d->norm[0] = N[0];
        d->norm[1] = N[1];
        d->norm[2] = N[2];
      }
    }
  }

#undef W3_MH
#undef W3_RI
#undef W3_CI
  free(pe);
  free(nh);
  free(nv);
  out->verts = v;
  out->nverts = static_cast<int>(o);
  out->gridverts = nquad * 6;
  return 1;
}

} // namespace outshine::Ground
#endif
