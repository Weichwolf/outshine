#ifndef TREELOOK_H
#define TREELOOK_H

namespace outshine {

/* Linear reflectance throughout. `BarkFreq` is furrow cycles per RADIAN of circumference, and
 * `LeafRgb` is already tint x base — that multiplication is world knowledge, so nothing downstream
 * may repeat it. The `Leaf*` block is TreeLeaf's `ProfileWidth` verbatim, which is why the drawn
 * silhouette is the declared lamina and no atlas has to carry a leaf shape. */
struct TreeLook {
  float BarkRgb[3] = {0.40f, 0.31f, 0.23f};
  float BarkDark = 0.62f;
  float BarkFreq = 4.0f;
  float BarkRidge = 0.2f;
  float LeafRgb[3] = {0.068f, 0.107f, 0.027f};
  float LeafWidth = 0.34f;
  float LeafWidest = 0.45f;
  float LeafTip = 0.5f;
  float LeafBaseFill = 0.0f;
  float LeafLobes = 0.0f;
  float LeafLobeDepth = 0.0f;
  float LeafSerration = 0.0f;
  float LeafFold = 0.10f;
  float NeedleWidth = 0.0f;   /* > 0 selects the needle profile */
};

} // namespace outshine
#endif /* TREELOOK_H */
