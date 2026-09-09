#include "import/GltfImporter.h"

#include <array>
#include <cmath>
#include <expected>
#include "Extent.h"
#include "Viewing.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
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

namespace Says {
constexpr auto InvalidAnimationTime = "animation time must be finite and nonnegative";
constexpr auto MissingSampledMaterial = "sampled material is absent from native geometry";
constexpr auto InvalidMaterialFactor =
    "sampled material factor is outside its finite unit interval";
constexpr auto EmissionOverflow = "sampled emission exceeds native finite storage";
constexpr auto MaterialPublicationFailed = "sampled material could not be published";
}

[[nodiscard]] std::expected<Material, std::string>
ApplyMaterialFactor(Material material, const Gltf::Pose::FactorAt &factor, double strength) {
  const size_t components = Gltf::FactorComponents(factor.Factor);
  for (size_t channel = 0; channel < components; ++channel) {
    if (!std::isfinite(factor.Values[channel]) || factor.Values[channel] < 0.0 ||
        factor.Values[channel] > 1.0) {
      return std::unexpected(std::string(Says::InvalidMaterialFactor));
    }
  }
  switch (factor.Factor) {
    case Gltf::MaterialFactor::BaseColour:
      for (size_t channel = 0; channel < 4; ++channel) {
        material.BaseColour[channel] = static_cast<float>(factor.Values[channel]);
      }
      break;
    case Gltf::MaterialFactor::Metalness:
      material.Metalness = static_cast<float>(factor.Values[0]);
      break;
    case Gltf::MaterialFactor::Roughness:
      material.Roughness = static_cast<float>(factor.Values[0]);
      break;
    case Gltf::MaterialFactor::Emissive:
      for (size_t channel = 0; channel < 3; ++channel) {
        const double value = factor.Values[channel] * strength;
        if (!std::isfinite(value) || value > std::numeric_limits<float>::max()) {
          return std::unexpected(std::string(Says::EmissionOverflow));
        }
        material.Emission[channel] = static_cast<float>(value);
      }
      break;
  }
  return material;
}

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

struct GltfImporter::Held {
  Gltf::Document File;
  Gltf::Subject Assembled;
  Gltf::Pose Motion;
  Gltf::VariantSelection Variant;
  Geometry Handed;
  Scenario::Camera Eye;
  std::vector<Gltf::Transform> Locals;
  std::vector<double> Weights;
  std::vector<Gltf::Pose::FactorAt> Factors;
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
    if (!Wears() || !SampleMaterials(seconds)) { return false; }
    HasEye = Camera(0, Eye);
    return true;
  }

  [[nodiscard]] bool SampleMaterials(double seconds) {
    if (!Moves) { return true; }
    Motion.FactorsAt(seconds, Factors);
    for (const auto &factor : Factors) {
      if (factor.Material < 0 || factor.Material >= Handed.surfaces()) {
        Why = Says::MissingSampledMaterial;
        return false;
      }
      const MaterialInstance index(factor.Material);
      auto sampled = ApplyMaterialFactor(
          Handed.surfaceAt(index),
          factor,
          File.Materials()[static_cast<size_t>(factor.Material)].EmissiveStrength);
      if (!sampled) {
        Why = std::move(sampled.error());
        return false;
      }
      if (!Handed.setSurface(index, *sampled)) {
        Why = Says::MaterialPublicationFailed;
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool Camera(int index, Scenario::Camera &out) const {
    Render::Viewpoint placed;
    std::string why;
    const std::span<const Gltf::Transform> locals =
        Moves ? Locals : std::span<const Gltf::Transform>{};
    if (!Gltf::DeclaredPlacement(File, index, placed, why, locals)) { return false; }
    Render::CameraOf(placed, out);
    return true;
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
      const Gltf::Material &declared = File.Materials()[static_cast<size_t>(index)];

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

GltfImporter::GltfImporter() : Held_(std::make_unique<Held>()) {}

GltfImporter::~GltfImporter() = default;
GltfImporter::GltfImporter(GltfImporter &&) noexcept = default;
GltfImporter &GltfImporter::operator=(GltfImporter &&) noexcept = default;

std::expected<void, std::string> GltfImporter::load(std::string_view path) {
  auto candidate = std::make_unique<Held>();
  const auto refuse = [&](std::string why) -> std::expected<void, std::string> {
    if (!Held_) { Held_ = std::make_unique<Held>(); }
    Held_->Why = why;
    return std::unexpected(std::move(why));
  };
  if (!candidate->File.ReadFile(path)) { return refuse(candidate->File.Error()); }
  if (!candidate->Assemble(0.0)) { return refuse(std::move(candidate->Why)); }
  Held_ = std::move(candidate);
  return {};
}

std::expected<void, std::string> GltfImporter::selectMaterialVariant(std::string_view variant) {
  Held &held = *Held_;
  const Gltf::VariantSelection wanted{std::string(variant)};
  int index = -1;
  if (!wanted.Against(held.File, index, held.Why)) { return std::unexpected(held.Why); }
  held.Variant = wanted;
  if (!held.Assemble(0.0)) { return std::unexpected(held.Why); }
  held.Why.clear();
  return {};
}

std::expected<void, std::string> GltfImporter::selectAnimations(std::span<const int> animations) {
  Held &held = *Held_;
  Gltf::Pose candidate;
  if (!animations.empty() && !Gltf::Pose::Build(held.File, animations, candidate, held.Why)) {
    return std::unexpected(held.Why);
  }
  held.Motion = std::move(candidate);
  held.Moves = held.Motion.Valid();
  if (!held.Assemble(0.0)) { return std::unexpected(held.Why); }
  held.Why.clear();
  return {};
}

const std::string &GltfImporter::error() const {
  return Held_->Why;
}

const Geometry &GltfImporter::geometry() const {
  return Held_->Handed;
}

int GltfImporter::animationCount() const {
  return static_cast<int>(Held_->File.Animations().size());
}

double GltfImporter::durationS() const {
  return Held_->Moves ? Held_->Motion.EndS() : 0.0;
}

std::expected<void, std::string> GltfImporter::sampleAnimation(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0) {
    Held_->Why = Says::InvalidAnimationTime;
    return std::unexpected(Held_->Why);
  }
  if (!Held_->Assemble(seconds)) { return std::unexpected(Held_->Why); }
  Held_->Why.clear();
  return {};
}

bool GltfImporter::hasDefaultCamera() const {
  return Held_->HasEye;
}

int GltfImporter::cameraCount() const {
  return static_cast<int>(Held_->File.Cameras().size());
}

bool GltfImporter::camera(int index, Scenario::Camera &out) const {
  return Held_->Camera(index, out);
}

std::expected<Scenario::Camera, GltfImporter::FrameError>
GltfImporter::frameCamera(Extent viewport) const noexcept {
  if (viewport.WidthPx <= 0 || viewport.HeightPx <= 0) {
    return std::unexpected(FrameError::InvalidViewport);
  }
  Render::Viewpoint fitted;
  const double aspect = static_cast<double>(viewport.WidthPx) / viewport.HeightPx;
  if (!Held_ || !Held_->Assembled.Frame(fitted, Render::kFramingFill, aspect)) {
    return std::unexpected(FrameError::InvalidBounds);
  }
  Scenario::Camera camera;
  Render::CameraOf(fitted, camera);
  return camera;
}

const Scenario::Camera &GltfImporter::camera() const {
  return Held_->Eye;
}

}
