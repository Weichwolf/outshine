#include "Loaded.h"

#include <vector>

#include "Document.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"

namespace outshine {

struct Loaded::Held {
  Gltf::Document File;
  Gltf::Subject Assembled;
  Gltf::Pose Motion;
  Gltf::VariantSelection Variant;
  Geometry Handed;
  Camera Eye;
  std::vector<Gltf::Transform> Locals;
  std::vector<double> Weights;
  std::vector<int> Plays;
  std::string Why;
  bool HasEye = false;
  bool Moves = false;

  [[nodiscard]] bool Assemble(double seconds) {
    const bool built =
        Moves ? (Motion.At(seconds, Locals, Weights),
                 Assembled.Build(File, Span<const Gltf::Transform>(Locals.data(), Locals.size()),
                                 Span<const double>(Weights.data(), Weights.size()), Variant))
              : Assembled.Build(File, Variant);
    if (!built) {
      Why = Assembled.Error();
      return false;
    }
    Handed = Assembled.Handed(File);
    return true;
  }
};

Loaded::Loaded() : Held_(std::make_unique<Held>()) {}
Loaded::~Loaded() = default;
Loaded::Loaded(Loaded &&) noexcept = default;
Loaded &Loaded::operator=(Loaded &&) noexcept = default;

bool Loaded::reads(std::string_view path) {
  Held &held = *Held_;
  if (!held.File.ReadFile(std::string(path))) {
    held.Why = held.File.Error();
    return false;
  }
  held.Plays.clear();
  held.Moves = false;

  // THE FILE'S OWN CAMERA IS READ ONCE AND KEPT. A glTF may place several; the FIRST is the one a
  // client means by "the camera this asset ships", and a client that wants another declares it.
  held.HasEye = false;
  if (!held.File.Cameras().empty()) {
    Render::Viewpoint placed;
    std::string ignored;
    if (Gltf::DeclaredPlacement(held.File, 0, placed, ignored)) {
      Camera &eye = held.Eye;
      eye.Placed = true;
      eye.LooksAt = true;
      for (int axis = 0; axis < 3; ++axis) {
        eye.Stands.AtM[axis] = placed.EyeM[axis];
        eye.LookAtM[axis] = placed.EyeM[axis] + placed.Forward[axis];
        eye.UpM[axis] = placed.Up[axis];
      }
      if (placed.Kind == Render::CameraKind::Orthographic) {
        eye.setProjection(-placed.XMagM, placed.XMagM, -placed.YMagM, placed.YMagM, placed.ZNearM,
                          placed.ZFarM);
      } else {
        eye.setProjection(placed.YfovRad * 180.0 / 3.14159265358979323846, placed.ZNearM,
                          placed.ZFarM);
      }
      held.HasEye = true;
    }
  }
  return held.Assemble(0.0);
}

bool Loaded::wears(std::string_view variant) {
  Held &held = *Held_;
  Gltf::VariantSelection wanted{std::string(variant)};
  int index = -1;
  if (!wanted.Against(held.File, index, held.Why)) { return false; }
  held.Variant = wanted;
  return held.Assemble(0.0);
}

bool Loaded::plays(std::span<const int> animations) {
  Held &held = *Held_;
  held.Plays.assign(animations.begin(), animations.end());
  if (held.Plays.empty()) {
    held.Moves = false;
    return held.Assemble(0.0);
  }
  if (!Gltf::Pose::Build(held.File, Span<const int>(held.Plays.data(), held.Plays.size()),
                         held.Motion, held.Why)) {
    return false;
  }
  held.Moves = held.Motion.Valid();
  return held.Assemble(0.0);
}

const std::string &Loaded::error(void) const { return Held_->Why; }
const Geometry &Loaded::geometry(void) const { return Held_->Handed; }
int Loaded::animations(void) const { return (int)Held_->File.Animations().size(); }
double Loaded::durationS(void) const { return Held_->Moves ? Held_->Motion.EndS() : 0.0; }
bool Loaded::poses(double seconds) { return Held_->Assemble(seconds); }
bool Loaded::carriesCamera(void) const { return Held_->HasEye; }
const Camera &Loaded::camera(void) const { return Held_->Eye; }

}
