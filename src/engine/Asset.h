#ifndef OUTSHINE_ENGINE_ASSET_H
#define OUTSHINE_ENGINE_ASSET_H

#include <string>
#include <cmath>
#include <vector>

#include <Geometry.h>
#include <Scenario.h>

#include "Document.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"

namespace outshine::Core {

class Posed {
public:
  void Clears();
  [[nodiscard]] bool Reads(const std::string &path, const std::string &variant,
                           AssetAnimation animation, int clip, double fps, std::string &error);
  [[nodiscard]] bool Poses(double seconds, std::string &error);
  void Carries(const Gltf::Subject &built) { Assembled_ = built; }

  // THE WORLD'S GEOMETRY IS MOVED IN, NEVER COPIED. A scenario's ground is the door's own
  // `Geometry` -- float per part, which is what the device binds -- and the engine used to widen
  // it into a `Gltf::Subject` and then narrow it back. Taking OWNERSHIP is what makes the copy
  // unnecessary: the producer builds it, hands it over, and the parts of the render shape view it
  // in place for as long as it stands.
  void Carries(outshine::Geometry &&built) {
    Built_ = std::move(built);
    HoldsBuilt_ = true;
  }
  [[nodiscard]] bool HoldsBuilt() const { return HoldsBuilt_; }
  [[nodiscard]] const outshine::Geometry &Built() const { return Built_; }

  [[nodiscard]] const Gltf::Document &File() const { return File_; }
  // A NAME IS A PROMISE AND THIS ONE BROKE IT. It read `Geometry()` and returns a `Gltf::Subject`,
  // which is the ASSEMBLED form -- one flat buffer with parts as ranges and the pose already baked --
  // and NOT the door's `outshine::Geometry`, which owns per-part vectors and is what an author
  // fills. Two different types, one of the two names, on the accessor every engine file reaches
  // through. An hour of this session went into working out why a cooker taking a `Geometry` could
  // not be wired to a path that appeared to carry one; it does not, and now it does not say so
  // either.
  [[nodiscard]] const Gltf::Subject &Assembled() const { return Assembled_; }
  [[nodiscard]] Gltf::Subject &Assembled() { return Assembled_; }
  [[nodiscard]] bool Measures(double seconds, std::string &error);
  [[nodiscard]] const std::vector<double> &Previous() const { return PreviousPositionsM_; }
  [[nodiscard]] bool Moves() const { return Moves_; }
  [[nodiscard]] bool Stands() const { return Read_; }
  [[nodiscard]] int Frames() const { return Frames_; }
  // AN ANIMATION IS ADDRESSED IN SECONDS, NOT IN FRAMES. It ran on an index that wrapped at
  // `(int)(EndS * fps + 0.5)` -- Khronos's Fox declares 3.41666675 s, which rounds to THREE frames
  // at one per second, so the fourth second jumped back to the rest pose and lost 0.41666675 s of
  // motion that the file states. glTF says sampling past the last keyframe CLAMPS to it, which is
  // what the oracle renders and what this now does; looping is a client's policy and this door
  // does not yet spell one, so it is not invented here.
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
  Gltf::Pose Motion_;
  Gltf::VariantSelection Variant_;
  std::vector<Gltf::Transform> Locals_;
  std::vector<double> Weights_;
  [[nodiscard]] bool PoseInto(double seconds, bool records, std::string &error);
  std::vector<double> PreviousPositionsM_;
  bool Moves_ = false;
  bool Read_ = false;
  int Frames_ = 1;
  double AtS_ = 0.0;
};

}
#endif
