#ifndef OUTSHINE_ENGINE_ASSET_H
#define OUTSHINE_ENGINE_ASSET_H

#include <string>
#include <cmath>
#include <vector>

#include <scene/Geometry.h>
#include <scenario/Scenario.h>

#include "Document.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"

namespace outshine::Core {

class Posed {
public:
  void Clears();
  [[nodiscard]] bool Reads(const std::string &path,
                           const std::string &variant,
                           Scenario::AssetAnimation animation,
                           int clip,
                           double fps,
                           std::string &error);
  [[nodiscard]] bool Poses(double seconds, std::string &error);

  void Carries(const Gltf::Subject &built) {
    Assembled_ = built;
    Changed_ += 1;
  }

  void Carries(outshine::Geometry &&built) {
    Built_ = std::move(built);
    HoldsBuilt_ = true;
    Changed_ += 1;
  }

  [[nodiscard]] bool Appends(const Gltf::Subject &more) {
    Changed_ += 1;
    return Assembled_.Append(more);
  }

  [[nodiscard]] uint64_t Changed() const { return Changed_; }

  [[nodiscard]] bool HoldsBuilt() const { return HoldsBuilt_; }

  [[nodiscard]] const outshine::Geometry &Built() const { return Built_; }

  [[nodiscard]] const Gltf::Document &File() const { return File_; }

  [[nodiscard]] const Gltf::Subject &Assembled() const { return Assembled_; }

  [[nodiscard]] bool Measures(double seconds, std::string &error);

  [[nodiscard]] const std::vector<double> &Previous() const { return PreviousPositionsM_; }

  [[nodiscard]] bool Moves() const { return Moves_; }

  [[nodiscard]] double LocalsDigest() const { return LocalsDigest_; }

  [[nodiscard]] double AssembledDigest() const { return AssembledDigest_; }

  [[nodiscard]] bool Stands() const { return Read_; }

  [[nodiscard]] int Frames() const { return Frames_; }

  [[nodiscard]] double AtS() const { return AtS_; }

  [[nodiscard]] double DurationS() const { return Motion_.EndS(); }

  void Advances(double stepS, bool loops) {
    const double end = Motion_.EndS();
    const double next = AtS_ + stepS;
    if (!(end > 0.0)) {
      AtS_ = 0.0;
      return;
    }
    AtS_ = loops ? next - end * std::floor(next / end) : (next > end ? end : next);
  }

private:
  Gltf::Document File_;
  Gltf::Subject Assembled_;
  outshine::Geometry Built_;
  bool HoldsBuilt_ = false;
  uint64_t Changed_ = 0;
  Gltf::Pose Motion_;
  Gltf::VariantSelection Variant_;
  std::vector<Gltf::Transform> Locals_;
  std::vector<double> Weights_;
  [[nodiscard]] bool PoseInto(double seconds, bool records, std::string &error);
  std::vector<double> PreviousPositionsM_;
  bool Moves_ = false;
  double LocalsDigest_ = 0.0;
  double AssembledDigest_ = 0.0;
  bool Read_ = false;
  int Frames_ = 1;
  double AtS_ = 0.0;
};

} // namespace outshine::Core
#endif
