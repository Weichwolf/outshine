/* THE SURFACE THE RENDERER DRAWS, as a height field and in one place. ChunkBuildEcef lays its nodes
 * down through these functions and emits ChunkQuadWinding's corners; the height oracle evaluates the
 * plane of the triangle that same winding names. "Same posting indices, same triangle split" is
 * therefore a property of the code, not an observation about two numbers that happen to agree. */
#ifndef CHUNKSURFACE_H
#define CHUNKSURFACE_H
#include <array>
#include <stdint.h>

namespace outshine::World {

/* Nodes along one chunk edge. `grid` is the requested quad count; a chunk never carries more nodes
 * than the source has postings. */
inline int ChunkNodes(uint32_t postings, int grid) {
  const int wanted = (grid < 2 ? 2 : grid) + 1;
  return (int)postings < wanted ? (int)postings : wanted;
}

/* Which SOURCE posting node k stands on. The truncation is the point: the spacing alternates between
 * one and two postings, so a reader must ask instead of dividing. */
inline uint32_t ChunkNodePosting(int k, uint32_t postings, int nodes) {
  return (uint32_t)((long)k * (long)(postings - 1u) / (long)(nodes - 1));
}

/* The node cell holding source posting `p`: the largest k with ChunkNodePosting(k) <= p. Inverting
 * the truncation exactly — floor(k*span/(N-1)) <= floor(p) is k*span < (floor(p)+1)*(N-1) — costs a
 * ceiling division and never a search. */
inline int ChunkNodeCell(double posting, uint32_t postings, int nodes) {
  const long span = (long)postings - 1, last = (long)nodes - 2;
  if (span <= 0 || last <= 0) return 0;
  long p = (long)posting;
  if (posting < 0.0) p = 0;
  else if (p > span) p = span;
  long k = ((p + 1) * (long)(nodes - 1) + span - 1) / span - 1;
  if (k < 0) k = 0;
  else if (k > last) k = last;
  return (int)k;
}

/* THE SPLIT, and the only statement of it: two triangles by their corners, row 0 north, col 0 west.
 * Every reader of the surface goes through here. */
struct ChunkQuadCorner { int Row, Col; };
inline const std::array<ChunkQuadCorner, 6> &ChunkQuadWinding() {
  static const std::array<ChunkQuadCorner, 6> kCorners{
      {{0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {1, 0}}};
  return kCorners;
}

/* HOW FAR APART THE TWO EVALUATORS OF THIS SURFACE MAY STAND, along the local vertical, in metres.
 * Not a tolerance anyone chose: the sum of what separates ChunkBuildEcef's triangle from
 * ChunkCellHeight's plane at the same place — the oracle's float32 sum at the corner height and at
 * the cell's own drop, the builder's float32 vertex offset and its float32 (origin - eye), the
 * shader's float32 add, the oracle being linear in Mercator fraction where the builder is linear in
 * ECEF, and the builder's chord sitting under the geodetic arc. Envelope: this surface (z14, 128
 * cells), |h| <= 4096 m, the camera in the sampled tile, slope <= 80 deg. Derived — the instrument
 * that summed those terms and checked them against plumb runs is gone with tools/, so the number
 * currently stands on no reproducible derivation. A disagreement above this is a second
 * interpolant, not arithmetic. */
inline constexpr float kSurfaceAgreementM = 9.17e-4f;

struct ChunkCell;
inline float ChunkCellHeight(const ChunkCell &cell, float su, float sv);

/* One quad of a row-major node grid, addressed by its north-west node. THE CORNERS ARE REACHABLE
 * FROM ONE FUNCTION: four corner heights in a caller's hands are a second interpolant waiting to be
 * written, and the one this cell agrees with is below. */
struct ChunkCell {
  const float *Nodes;   /* heights in metres on the DEM's datum */
  int Stride;
  int Row, Col;

private:
  friend float ChunkCellHeight(const ChunkCell &cell, float su, float sv);
  float At(int row, int col) const {
    return Nodes[(size_t)(Row + row) * (size_t)Stride + (size_t)(Col + col)];
  }
};

/* The drawn surface inside that quad: the PLANE OF THE WINDING TRIANGLE the point falls in, found by
 * its own barycentrics. su runs east across the cell, sv south. Turning kCorners therefore turns
 * this answer with the emitted geometry — the split is asked for, never restated. */
inline float ChunkCellHeight(const ChunkCell &cell, float su, float sv) {
  const std::array<ChunkQuadCorner, 6> &w = ChunkQuadWinding();
  float height = 0.0f;
  for (int t = 0; t < 6; t += 3) {
    const ChunkQuadCorner a = w[t], b = w[t + 1], c = w[t + 2];
    const float bu = (float)(b.Col - a.Col), bv = (float)(b.Row - a.Row);
    const float cu = (float)(c.Col - a.Col), cv = (float)(c.Row - a.Row);
    const float pu = su - (float)a.Col, pv = sv - (float)a.Row;
    const float det = bu * cv - cu * bv;
    const float l1 = (pu * cv - pv * cu) / det, l2 = (bu * pv - bv * pu) / det;
    const float ha = cell.At(a.Row, a.Col);
    height = ha + l1 * (cell.At(b.Row, b.Col) - ha) + l2 * (cell.At(c.Row, c.Col) - ha);
    if (l1 >= 0.0f && l2 >= 0.0f && l1 + l2 <= 1.0f) break;
  }
  return height;
}

} // namespace outshine::World
#endif /* CHUNKSURFACE_H */
