#include "AnimationAsset.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine {

void AnimationAssetSet::Adopt(std::vector<AnimationAsset> &&animations) {
  Animations_ = std::move(animations);
}

bool AnimationAssetSet::Select(std::span<const int> animations,
                               AnimationClip &out,
                               std::string &error) const {
  if (animations.empty()) {
    out = AnimationClip();
    error.clear();
    return true;
  }
  for (const int animation : animations) {
    if (animation < 0 || static_cast<size_t>(animation) >= Animations_.size()) {
      error = "animation " + std::to_string(animation) + " of " +
              std::to_string(Animations_.size()) + " available";
      return false;
    }
    const std::string &failure = Animations_[static_cast<size_t>(animation)].Error;
    if (!failure.empty()) {
      error = failure;
      return false;
    }
  }
  const AnimationClip &basis = Animations_[static_cast<size_t>(animations.front())].Clip;
  std::vector<AnimationTrack> tracks;
  std::map<std::tuple<AnimationTarget, int>, int> claimed;
  double start = 0.0;
  double end = 0.0;
  bool first = true;
  for (const int animation : animations) {
    const AnimationClip &clip = Animations_[static_cast<size_t>(animation)].Clip;
    for (const AnimationTrack &track : clip.Tracks_) {
      const auto [found, inserted] =
          claimed.emplace(std::tuple{track.Property, track.Target}, animation);
      if (!inserted) {
        error = "animations " + std::to_string(found->second) + " and " +
                std::to_string(animation) + " drive the same target";
        return false;
      }
      tracks.push_back(track);
    }
    start = first ? clip.StartS_ : std::min(start, clip.StartS_);
    end = first ? clip.EndS_ : std::max(end, clip.EndS_);
    first = false;
  }
  AnimationClip candidate;
  candidate.Adopt(std::vector<AnimationRestPose>(basis.Nodes_),
                  std::vector<double>(basis.RestWeights_),
                  std::move(tracks),
                  {.StartS = start, .EndS = end});
  out = std::move(candidate);
  error.clear();
  return true;
}

}
