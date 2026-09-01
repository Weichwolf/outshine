#include "Heap.h"
#include <bit>
#include <string>
#include <cstdint>

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
  Changed_ += 1;
}

bool Posed::Reads(const std::string &path,
                  const std::string &variant,
                  AssetAnimation animation,
                  int clip,
                  double fps,
                  std::string &error) {
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
    Frames_ = Moves_ ? static_cast<int>(Motion_.EndS() * fps + 0.5) : 1;
    if (Frames_ < 1) { Frames_ = 1; }
  }
  Read_ = true;
  Changed_ += 1;
  return true;
}

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
    {
      uint64_t keyed = 1469598103934665603ull;
      for (const Gltf::Transform &one : Locals_) {
        for (const double part : one.M) {
          keyed = (keyed ^ std::bit_cast<uint64_t>(part)) * 1099511628211ull;
        }
      }
      LocalsDigest_ = static_cast<double>(keyed % 1000000007ull);
    }
    const Heap::Tagged building("pose-build");
    if (Assembled_.Build(File_,
                         Span<const Gltf::Transform>(Locals_.data(), Locals_.size()),
                         Span<const double>(Weights_.data(), Weights_.size()),
                         Variant_)) {
      Changed_ += 1;
      if (first && records) { PreviousPositionsM_ = Assembled_.PositionsM(); }
      {
        uint64_t keyed = 1469598103934665603ull;
        for (const double part : Assembled_.PositionsM()) {
          keyed = (keyed ^ std::bit_cast<uint64_t>(part)) * 1099511628211ull;
        }
        AssembledDigest_ = static_cast<double>(keyed % 1000000007ull);
      }
      return true;
    }
  } else if (Assembled_.Build(File_, Variant_)) {
    Changed_ += 1;
    return true;
  }
  error = Assembled_.Error();
  return false;
}

} // namespace outshine::Core
