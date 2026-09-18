#ifndef OUTSHINE_CONTENT_ANIMATION_ANIMATIONASSET_H
#define OUTSHINE_CONTENT_ANIMATION_ANIMATIONASSET_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "AnimationClip.h"

namespace outshine {

struct AnimationAsset {
  AnimationClip Clip;
  std::string Error;
};

class AnimationAssetSet {
public:
  void Adopt(std::vector<AnimationAsset> &&animations);
  [[nodiscard]] bool
  Select(std::span<const int> animations, AnimationClip &out, std::string &error) const;

  [[nodiscard]] size_t Count() const noexcept { return Animations_.size(); }

private:
  std::vector<AnimationAsset> Animations_;
};

}

#endif
