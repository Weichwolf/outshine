#ifndef OUTSHINE_WORLD_GROUND_CHUNKSURFACE_H
#define OUTSHINE_WORLD_GROUND_CHUNKSURFACE_H
#include <array>
#include <utility>
#include <stdint.h>

#include <cstdint>

namespace outshine::Ground {

struct Posted {
  uint32_t Postings = 0;
  int Grid = 0;
};

inline int ChunkNodes(Posted over) {
  const uint32_t postings = over.Postings;
  const int wanted = (over.Grid < 2 ? 2 : over.Grid) + 1;
  return std::cmp_less(postings, wanted) ? static_cast<int>(postings) : wanted;
}

inline uint32_t ChunkNodePosting(int k, uint32_t postings, int nodes) {
  return static_cast<uint32_t>(static_cast<long>(k) * static_cast<long>(postings - 1u) /
                               static_cast<long>(nodes - 1));
}

struct Sampling {
  int Side = 0;
  uint32_t Postings = 0;
};

inline int ChunkNodeCell(double posting, Sampling over) {
  const long span = static_cast<long>(over.Postings) - 1;
  const long last = static_cast<long>(over.Side) - 2;
  if (span <= 0 || last <= 0) { return 0; }
  long p = static_cast<long>(posting);
  if (posting < 0.0) {
    p = 0;
  } else if (p > span) {
    p = span;
  }
  long k = ((p + 1) * static_cast<long>(over.Side - 1) + span - 1) / span - 1;
  if (k < 0) {
    k = 0;
  } else if (k > last) {
    k = last;
  }
  return static_cast<int>(k);
}

struct ChunkQuadCorner {
  int Row, Col;
};

inline const std::array<ChunkQuadCorner, 6> &ChunkQuadWinding() {
  static const std::array<ChunkQuadCorner, 6> kCorners{{{.Row = 0, .Col = 0},
                                                        {.Row = 0, .Col = 1},
                                                        {.Row = 1, .Col = 1},
                                                        {.Row = 0, .Col = 0},
                                                        {.Row = 1, .Col = 1},
                                                        {.Row = 1, .Col = 0}}};
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

  [[nodiscard]] float At(int row, int col) const {
    return Nodes[static_cast<size_t>(Row + row) * static_cast<size_t>(Stride) +
                 static_cast<size_t>(Col + col)];
  }
};

inline float ChunkCellHeight(const ChunkCell &cell, float su, float sv) {
  const std::array<ChunkQuadCorner, 6> &w = ChunkQuadWinding();
  float height = 0.0f;
  for (int t = 0; t < 6; t += 3) {
    const ChunkQuadCorner a = w[t];
    const ChunkQuadCorner b = w[t + 1];
    const ChunkQuadCorner c = w[t + 2];
    const auto bu = static_cast<float>(b.Col - a.Col);
    const auto bv = static_cast<float>(b.Row - a.Row);
    const auto cu = static_cast<float>(c.Col - a.Col);
    const auto cv = static_cast<float>(c.Row - a.Row);
    const float pu = su - static_cast<float>(a.Col);
    const float pv = sv - static_cast<float>(a.Row);
    const float det = bu * cv - cu * bv;
    const float l1 = (pu * cv - pv * cu) / det;
    const float l2 = (bu * pv - bv * pu) / det;
    const float ha = cell.At(a.Row, a.Col);
    height = ha + l1 * (cell.At(b.Row, b.Col) - ha) + l2 * (cell.At(c.Row, c.Col) - ha);
    if (l1 >= 0.0f && l2 >= 0.0f && l1 + l2 <= 1.0f) { break; }
  }
  return height;
}

} // namespace outshine::Ground
#endif
