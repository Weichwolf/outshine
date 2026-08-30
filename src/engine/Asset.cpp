#include "Heap.h"
#include "Asset.h"

#include "Span.h"

namespace outshine::Core {

void Posed::Clears() {
  Assembled_ = Gltf::Subject();
  Built_.clear();
  HoldsBuilt_ = false;
  File_ = Gltf::Document();
  Read_ = false;
  Moves_ = false;
  Frames_ = 1;
  AtS_ = 0.0;
}

bool Posed::Reads(const std::string &path, const std::string &variant, AssetAnimation animation,
                  int clip, double fps, std::string &error) {
  if (Read_) { return true; }
  if (!variant.empty()) { Variant_ = Gltf::VariantSelection(variant); }
  if (!File_.ReadFile(path)) {
    error = File_.Error();
    return false;
  }
  if (!File_.Animations().empty() &&
      (animation == AssetAnimation::Play || animation == AssetAnimation::Loop)) {
    if (!Gltf::Pose::Build(File_, clip, Motion_, error)) { return false; }
    Moves_ = Motion_.EndS() > 0.0;
    Frames_ = Moves_ ? (int)(Motion_.EndS() * fps + 0.5) : 1;
    if (Frames_ < 1) { Frames_ = 1; }
  }
  Read_ = true;
  return true;
}

// A SWEEP THAT ONLY MEASURES MUST NOT DISTURB THE MOTION HISTORY. Deriving a camera over an
// animation poses every frame and poses back, and each of those wrote `PreviousPositionsM_` -- so
// the restored frame 0 carried the LAST frame as its predecessor and every covered pixel reported
// motion on a still. Measured on Khronos's AnimatedCube: 346.7 px of velocity per frame at frame
// 0, over all 97468 covered pixels.
bool Posed::Measures(double seconds, std::string &error) {
  return PoseInto(seconds, false, error);
}

bool Posed::Poses(double seconds, std::string &error) {
  return PoseInto(seconds, true, error);
}

bool Posed::PoseInto(double seconds, bool records, std::string &error) {
  if (Moves_) {
    const bool first = Assembled_.VertexCount() == 0;
    if (!first && records) {
      const Heap::Tagged copying("pose-previous");
      PreviousPositionsM_ = Assembled_.PositionsM();
    }
    Motion_.At(seconds, Locals_, Weights_);
    const Heap::Tagged building("pose-build");
    if (Assembled_.Build(File_, Span<const Gltf::Transform>(Locals_.data(), Locals_.size()),
                        Span<const double>(Weights_.data(), Weights_.size()), Variant_)) {
      if (first && records) { PreviousPositionsM_ = Assembled_.PositionsM(); }
      return true;
    }
  } else if (Assembled_.Build(File_, Variant_)) {
    return true;
  }
  error = Assembled_.Error();
  return false;
}

}
