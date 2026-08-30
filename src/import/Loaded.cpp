#include "Loaded.h"

#include <cstring>
#include <span>
#include <vector>

#include "Document.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"
#include "surface/Surfaces.h"

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
    return Wears();
  }

  // THE MAPS, TRANSLATED RATHER THAN RE-READ. `ResolveFileSurface` is the one place that decides
  // what a glTF texture reference MEANS -- which uv set, which wrap, which filter, what transform
  // -- and a second reading of the same references is how two answers to one question start. So
  // this asks it, and turns what it resolved into the door's own words.
  [[nodiscard]] bool Wears() {
    Render::SurfaceTable table;
    Gltf::ResolveSurfaceTable(File, Assembled, true, true, table);
    if (!Gltf::ResolveFileSurface(File, Assembled, Render::ColourFrom::Row,
                                  Render::ColourCarrier::Texture, table, Why)) {
      return false;
    }
    for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
      const int index = slot < table.Material.size() ? table.Material[slot] : -1;
      if (index < 0 || index >= Handed.surfaces()) { continue; }
      Material row = Handed.surfaceAt(MaterialInstance(index));
      const Render::SubjectMaterial &held = table.Slots[slot];
      const struct {
        const Render::SubjectTexture &From;
        SurfaceMap &Into;
      } maps[] = {
          {held.Colour, row.BaseColourMap},   {held.Normal, row.NormalMap},
          {held.MetalRough, row.MetalRoughMap}, {held.Emissive, row.EmissiveMap},
          {held.SpecularStrength, row.SpecularStrengthMap},
          {held.SpecularTint, row.SpecularTintMap}};
      for (const auto &map : maps) { Names(map.From, map.Into); }
      if (!Handed.setSurface(MaterialInstance(index), row)) {
        Why = "a surface the file declares could not be named on the geometry handed back";
        return false;
      }
    }
    return true;
  }

  void Names(const Render::SubjectTexture &from, SurfaceMap &into) {
    if (from.Rgba == nullptr || from.Width == 0 || from.Height == 0) { return; }
    into.Image = Keeps(from);
    into.Set = from.Set;
    into.Samples.Magnify =
        from.Magnify == Render::SubjectFilter::Nearest ? Filter::Nearest : Filter::Linear;
    into.Samples.Minify =
        from.Minify == Render::SubjectFilter::Nearest ? Filter::Nearest : Filter::Linear;
    into.Samples.Mip = from.Mip == Render::SubjectMip::None      ? MipFilter::None
                       : from.Mip == Render::SubjectMip::Nearest ? MipFilter::Nearest
                                                                 : MipFilter::Linear;
    const auto wrapped = [](Render::SubjectWrap held) {
      return held == Render::SubjectWrap::ClampToEdge     ? Wrap::ClampToEdge
             : held == Render::SubjectWrap::MirroredRepeat ? Wrap::MirroredRepeat
                                                           : Wrap::Repeat;
    };
    into.Samples.WrapU = wrapped(from.WrapU);
    into.Samples.WrapV = wrapped(from.WrapV);
  }

  // ONE IMAGE PER DISTINCT PICTURE. Two surfaces sharing a glTF texture decode to two rasters on
  // the engine's side, one per slot; the door's table is the ASSET's, so an identical picture is
  // kept once and both maps name it.
  [[nodiscard]] int Keeps(const Render::SubjectTexture &from) {
    const size_t bytes = (size_t)from.Width * (size_t)from.Height * 4u;
    const std::span<const uint8_t> pixels(from.Rgba, bytes);
    for (int at = 0; at < Handed.images(); ++at) {
      const ImageView held = Handed.imageAt(at);
      if (held.WidthPx != (int)from.Width || held.HeightPx != (int)from.Height) { continue; }
      if (held.Rgba.size() >= bytes && std::memcmp(held.Rgba.data(), from.Rgba, bytes) == 0) {
        return at;
      }
    }
    return Handed.addImage((int)from.Width, (int)from.Height, pixels);
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
      Render::CameraOf(placed, held.Eye);
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
int Loaded::cameras(void) const { return (int)Held_->File.Cameras().size(); }

bool Loaded::camera(int index, Camera &out) const {
  Render::Viewpoint placed;
  std::string why;
  if (!Gltf::DeclaredPlacement(Held_->File, index, placed, why)) { return false; }
  Render::CameraOf(placed, out);
  return true;
}

bool Loaded::frames(double fill, Camera &out) const {
  Render::Viewpoint fitted;
  if (!Held_->Assembled.Frame(fitted, fill)) { return false; }
  Render::CameraOf(fitted, out);
  return true;
}

bool Loaded::frames(Camera &out) const { return frames(Render::kFramingFill, out); }
const Camera &Loaded::camera(void) const { return Held_->Eye; }

}
