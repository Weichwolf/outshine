#ifndef CHUNKSURFACE_H
#define CHUNKSURFACE_H
#include <array>
#include <stdint.h>

namespace outshine::World {

inline int ChunkNodes(uint32_t postings, int grid) {
  const int wanted = (grid < 2 ? 2 : grid) + 1;
  return (int)postings < wanted ? (int)postings : wanted;
}

inline uint32_t ChunkNodePosting(int k, uint32_t postings, int nodes) {
  return (uint32_t)((long)k * (long)(postings - 1u) / (long)(nodes - 1));
}

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

struct ChunkQuadCorner { int Row, Col; };
inline const std::array<ChunkQuadCorner, 6> &ChunkQuadWinding() {
  static const std::array<ChunkQuadCorner, 6> kCorners{
      {{0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {1, 0}}};
  return kCorners;
}

inline constexpr float kSurfaceAgreementM = 9.17e-4f;

struct ChunkCell;
inline float ChunkCellHeight(const ChunkCell &cell, float su, float sv);

struct ChunkCell {
  const float *Nodes;
  int Stride;
  int Row, Col;

private:
  friend float ChunkCellHeight(const ChunkCell &cell, float su, float sv);
  float At(int row, int col) const {
    return Nodes[(size_t)(Row + row) * (size_t)Stride + (size_t)(Col + col)];
  }
};

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

}
#endif
