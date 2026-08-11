#ifndef MODELLADDER_H
#define MODELLADDER_H

namespace outshine {

/* The number of quad elements one sheet instance stands for. The WGSL `kSheetElements` — change
 * one, change both. */
constexpr int kElementsPerSheet = 16;

/* ONE LADDER FOR AN INSTANCED MODEL, and it is `ClusterDag`'s: a level carries a model-space error
 * and the cut is the same inequality every cluster answers. */
namespace ModelLadder {

/* Mesh levels. The impostor is the level above them, so a ladder has kLevels + 1 rungs. */
constexpr int kLevels = 4;

/* THE IMPOSTOR ATLAS CELL'S TEXEL IS THE ANCHOR: a model of height H baked into a cell this many
 * pixels across carries an error of H/kCellPx metres, which projects to one pixel at
 * d = H * f_px / kCellPx. Every mesh level below it halves that error. */
constexpr float kCellPx = 256.0f;

/* Level k's model-space error as a fraction of the model's own height; k == kLevels is the
 * impostor. */
constexpr float Error(int k) {
  return 1.0f / (kCellPx * (float)(1u << (unsigned)(kLevels - k)));
}

} // namespace ModelLadder
} // namespace outshine
#endif /* MODELLADDER_H */
