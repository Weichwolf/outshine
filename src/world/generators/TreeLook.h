#ifndef OUTSHINE_WORLD_GENERATORS_TREELOOK_H
#define OUTSHINE_WORLD_GENERATORS_TREELOOK_H

namespace outshine {

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
  float NeedleWidth = 0.0f;
};

}
#endif
