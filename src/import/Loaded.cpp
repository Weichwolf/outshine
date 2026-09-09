#include "scene/Loaded.h"

#include <array>
#include <expected>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Document.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"
#include "surface/Surfaces.h"

namespace outshine {
namespace {

MipFilter MipOf(Render::SubjectMip mip) {
  switch (mip) {
    case Render::SubjectMip::None: return MipFilter::None;
    case Render::SubjectMip::Nearest: return MipFilter::Nearest;
    case Render::SubjectMip::Linear: return MipFilter::Linear;
  }
  return MipFilter::Linear;
}

Wrap WrapOf(Render::SubjectWrap held) {
  switch (held) {
    case Render::SubjectWrap::ClampToEdge: return Wrap::ClampToEdge;
    case Render::SubjectWrap::MirroredRepeat: return Wrap::MirroredRepeat;
    case Render::SubjectWrap::Repeat: return Wrap::Repeat;
  }
  return Wrap::Repeat;
}

}

struct Loaded::Held {
  Gltf::Document File;
  Gltf::Subject Assembled;
  Gltf::Pose Motion;
  Gltf::VariantSelection Variant;
  Geometry Handed;
  Scenario::Camera Eye;
  std::vector<Gltf::Transform> Locals;
  std::vector<double> Weights;
  std::vector<int> Plays;
  std::string Why;
  bool HasEye = false;
  bool Moves = false;

  [[nodiscard]] bool Assemble(double seconds) {
    const bool built =
        Moves ? (Motion.At(seconds, Locals, Weights),
                 Assembled.Build(File,
                                 std::span<const Gltf::Transform>(Locals.data(), Locals.size()),
                                 std::span<const double>(Weights.data(), Weights.size()),
                                 Variant))
              : Assembled.Build(File, Variant);
    if (!built) {
      Why = Assembled.Error();
      return false;
    }
    Handed = Assembled.Handed(File);
    return Wears();
  }

  [[nodiscard]] bool Wears() {
    Render::SurfaceTable table;
    Gltf::ResolveSurfaceTable(File, Assembled, true, true, table);
    if (!Gltf::ResolveFileSurface(
            File, Assembled, Render::ColourFrom::Row, Render::ColourCarrier::Texture, table, Why)) {
      return false;
    }
    for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
      const int index = slot < table.Material.size() ? table.Material[slot] : -1;
      if (index < 0 || index >= Handed.surfaces() ||
          static_cast<size_t>(index) >= File.Materials().size()) {
        continue;
      }
      Material row = Handed.surfaceAt(MaterialInstance(index));
      const Render::SubjectMaterial &held = table.Slots[slot];
      const Gltf::MaterialRef &declared = File.Materials()[static_cast<size_t>(index)];

      struct MapRow {
        const Render::SubjectTexture &From;
        SurfaceMap &Into;
        const Gltf::TextureRef &Declared;
      };

      const std::array<MapRow, 6> maps = {
          {{.From = held.Colour, .Into = row.BaseColourMap, .Declared = declared.BaseColour},
           {.From = held.Normal, .Into = row.NormalMap, .Declared = declared.Normal},
           {.From = held.MetalRough,
            .Into = row.MetalRoughMap,
            .Declared = declared.MetallicRoughness},
           {.From = held.Emissive, .Into = row.EmissiveMap, .Declared = declared.Emissive},
           {.From = held.SpecularStrength,
            .Into = row.SpecularStrengthMap,
            .Declared = declared.SpecularStrength},
           {.From = held.SpecularTint,
            .Into = row.SpecularTintMap,
            .Declared = declared.SpecularTint}}};

      for (const auto &map : maps) {
        Names(map.From, map.Into);
        map.Into.Uv = map.Declared.Uv;
      }
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
    into.Sampler.Magnify =
        from.Magnify == Render::SubjectFilter::Nearest ? Filter::Nearest : Filter::Linear;
    into.Sampler.Minify =
        from.Minify == Render::SubjectFilter::Nearest ? Filter::Nearest : Filter::Linear;
    into.Sampler.Mip = MipOf(from.Mip);
    into.Sampler.WrapU = WrapOf(from.WrapU);
    into.Sampler.WrapV = WrapOf(from.WrapV);
  }

  [[nodiscard]] int Keeps(const Render::SubjectTexture &from) {
    const size_t bytes = static_cast<size_t>(from.Width) * static_cast<size_t>(from.Height) * 4u;
    const std::span<const uint8_t> pixels(from.Rgba, bytes);
    for (int at = 0; at < Handed.images(); ++at) {
      const ImageView held = Handed.imageAt(at);
      if (std::cmp_not_equal(held.WidthPx, from.Width) ||
          std::cmp_not_equal(held.HeightPx, from.Height)) {
        continue;
      }
      if (held.Rgba.size() >= bytes && std::memcmp(held.Rgba.data(), from.Rgba, bytes) == 0) {
        return at;
      }
    }
    return Handed.addImage(static_cast<int>(from.Width), static_cast<int>(from.Height), pixels);
  }
};

Loaded::Loaded() : Held_(std::make_unique<Held>()) {}

Loaded::~Loaded() = default;
Loaded::Loaded(Loaded &&) noexcept = default;
Loaded &Loaded::operator=(Loaded &&) noexcept = default;

std::expected<void, std::string> Loaded::load(std::string_view path) {
  auto candidate = std::make_unique<Held>();
  const auto refuse = [&](std::string why) -> std::expected<void, std::string> {
    if (!Held_) { Held_ = std::make_unique<Held>(); }
    Held_->Why = why;
    return std::unexpected(std::move(why));
  };
  if (!candidate->File.ReadFile(path)) { return refuse(candidate->File.Error()); }
  if (!candidate->File.Cameras().empty()) {
    Render::Viewpoint placed;
    std::string why;
    if (Gltf::DeclaredPlacement(candidate->File, 0, placed, why)) {
      Render::CameraOf(placed, candidate->Eye);
      candidate->HasEye = true;
    }
  }
  if (!candidate->Assemble(0.0)) { return refuse(std::move(candidate->Why)); }
  Held_ = std::move(candidate);
  return {};
}

bool Loaded::wears(std::string_view variant) {
  Held &held = *Held_;
  const Gltf::VariantSelection wanted{std::string(variant)};
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
  if (!Gltf::Pose::Build(held.File,
                         std::span<const int>(held.Plays.data(), held.Plays.size()),
                         held.Motion,
                         held.Why)) {
    return false;
  }
  held.Moves = held.Motion.Valid();
  return held.Assemble(0.0);
}

const std::string &Loaded::error() const {
  return Held_->Why;
}

const Geometry &Loaded::geometry() const {
  return Held_->Handed;
}

int Loaded::animations() const {
  return static_cast<int>(Held_->File.Animations().size());
}

double Loaded::durationS() const {
  return Held_->Moves ? Held_->Motion.EndS() : 0.0;
}

bool Loaded::poses(double seconds) {
  return Held_->Assemble(seconds);
}

bool Loaded::carriesCamera() const {
  return Held_->HasEye;
}

int Loaded::cameras() const {
  return static_cast<int>(Held_->File.Cameras().size());
}

bool Loaded::camera(int index, Scenario::Camera &out) const {
  Render::Viewpoint placed;
  std::string why;
  if (!Gltf::DeclaredPlacement(Held_->File, index, placed, why)) { return false; }
  Render::CameraOf(placed, out);
  return true;
}

bool Loaded::frames(double fill, Scenario::Camera &out) const {
  Render::Viewpoint fitted;
  if (!Held_->Assembled.Frame(fitted, fill)) { return false; }
  Render::CameraOf(fitted, out);
  return true;
}

bool Loaded::frames(Scenario::Camera &out) const {
  return frames(Render::kFramingFill, out);
}

const Scenario::Camera &Loaded::camera() const {
  return Held_->Eye;
}

}
