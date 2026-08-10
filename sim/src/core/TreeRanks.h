#ifndef TREERANKS_H
#define TREERANKS_H

namespace outshine {

/* The WGSL `kCardLeaves` — change one, change both. */
constexpr int kLeavesPerCard = 16;

namespace TreeRank {

/* Their number is the only thing about the mesh ranks that is chosen. */
constexpr int kCount = 4;

/* THE IMPOSTOR'S TEXEL IS THE MODEL-SPACE ERROR every mesh rank is measured against: a tree of
 * height H baked into a cell of this many pixels carries an error of H/kCellPx metres, and that
 * projects to one pixel at d = H * f_px / kCellPx. That inequality, with lambda = H/kCellPx, is the
 * only thing that decides where the mesh stops. */
constexpr float kCellPx = 256.0f;

/* One pixel at rank k's NEAREST stand, as a fraction of the tree's height. The impostor's cell is
 * the anchor and every rank below it halves. */
constexpr float Pixel(int k) {
  return 1.0f / (kCellPx * (float)(1u << (unsigned)(kCount - k)));
}

} // namespace TreeRank
} // namespace outshine
#endif /* TREERANKS_H */
