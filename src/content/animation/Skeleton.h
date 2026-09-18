#ifndef OUTSHINE_CONTENT_ANIMATION_SKELETON_H
#define OUTSHINE_CONTENT_ANIMATION_SKELETON_H

#include <cstdint>
#include <vector>

#include "AffineTransform.h"

namespace outshine {

struct Skeleton {
  std::vector<uint32_t> JointNodes;
  std::vector<AffineTransform> InverseBind;
  int RootNode = -1;
};

}
#endif
