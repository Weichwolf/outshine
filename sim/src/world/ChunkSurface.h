/* THE SURFACE THE RENDERER DRAWS, as a height field and in one place. ChunkBuildEcef lays its nodes
 * down through these functions and cuts every quad along this diagonal; the height oracle reads the
 * same three. "Same posting indices, same triangle split" is therefore a property of the code, not
 * an observation about two numbers that happen to agree today. */
#ifndef CHUNKSURFACE_H
#define CHUNKSURFACE_H
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

/* THE SPLIT: the quad is cut from its north-west node to its south-east one, so `su >= sv` names the
 * northern triangle. su runs east across the cell, sv south; heights in metres on the DEM's datum. */
inline float ChunkTriangleHeight(float h00, float h10, float h11, float h01, float su, float sv) {
  return (su >= sv) ? h00 + (h10 - h00) * su + (h11 - h10) * sv
                    : h00 + (h11 - h01) * su + (h01 - h00) * sv;
}

/* The same split as the six vertices that are drawn: {NW,NE,SE} is the su >= sv triangle, {NW,SE,SW}
 * the other. The evaluator above and the emitted winding read one array, so a diagonal cannot be
 * turned in one of them alone. */
struct ChunkQuadCorner { int Row, Col; };
inline const ChunkQuadCorner *ChunkQuadWinding() {
  static const ChunkQuadCorner kCorners[6] = {{0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {1, 0}};
  return kCorners;
}

} // namespace outshine::World
#endif /* CHUNKSURFACE_H */
