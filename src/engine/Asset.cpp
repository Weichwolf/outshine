#include "Asset.h"

#include "Span.h"

namespace outshine::Core {

void Posed::Clears() {
  Geometry_ = Gltf::Subject();
  File_ = Gltf::Document();
  Read_ = false;
  Moves_ = false;
  Frames_ = 1;
  At_ = 0;
}

bool Posed::Reads(const std::string &path, const std::string &variant, AssetAnimation animation,
                  double fps, std::string &error) {
  if (Read_) { return true; }
  if (!variant.empty()) { Variant_ = Gltf::VariantSelection(variant); }
  if (!File_.ReadFile(path)) {
    error = File_.Error();
    return false;
  }
  if (!File_.Animations().empty() && animation == AssetAnimation::Play) {
    std::vector<int> all((size_t)File_.Animations().size());
    for (size_t at = 0; at < all.size(); ++at) { all[at] = (int)at; }
    if (!Gltf::Pose::Build(File_, Span<const int>(all.data(), all.size()), Motion_, error)) {
      return false;
    }
    Moves_ = Motion_.EndS() > 0.0;
    Frames_ = Moves_ ? (int)(Motion_.EndS() * fps + 0.5) : 1;
    if (Frames_ < 1) { Frames_ = 1; }
  }
  Read_ = true;
  return true;
}

bool Posed::Poses(int frame, double fps, std::string &error) {
  if (Moves_) {
    const bool first = Geometry_.VertexCount() == 0;
    if (!first) { PreviousPositionsM_ = Geometry_.PositionsM(); }
    Motion_.At((double)frame / fps, Locals_, Weights_);
    if (Geometry_.Build(File_, Span<const Gltf::Transform>(Locals_.data(), Locals_.size()),
                        Span<const double>(Weights_.data(), Weights_.size()), Variant_)) {
      if (first) { PreviousPositionsM_ = Geometry_.PositionsM(); }
      return true;
    }
  } else if (Geometry_.Build(File_, Variant_)) {
    return true;
  }
  error = Geometry_.Error();
  return false;
}

}
