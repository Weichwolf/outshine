#include "import/GltfImporter.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include "Extent.h"
#include <cstdint>
#include <memory>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Document.h"
#include "CameraFraming.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"
#include "native/NativeMaterials.h"

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

}

struct GltfImporter::Held {
  Gltf::Document File;
  Gltf::Subject Assembled;
  Gltf::Pose Motion;
  Gltf::VariantSelection Variant;
  Geometry Handed;
  std::vector<AffineTransform> Locals;
  std::vector<AffineTransform> PublishedLocals;
  std::vector<double> Weights;
  std::vector<Gltf::Pose::FactorAt> Factors;
  std::string Why;
  bool Moves = false;

  [[nodiscard]] bool Assemble(double seconds) {
    const bool built =
        Moves ? (Motion.At(seconds, Locals, Weights),
                 Assembled.Build(File,
                                 std::span<const AffineTransform>(Locals.data(), Locals.size()),
                                 std::span<const double>(Weights.data(), Weights.size()),
                                 Variant))
              : Assembled.Build(File, Variant);
    if (!built) {
      Why = Assembled.Error();
      return false;
    }
    auto converted = Assembled.Handed(File);
    if (!converted) {
      Why = std::move(converted.error());
      return false;
    }
    if (!Wears(*converted) || !SampleMaterials(seconds, *converted)) { return false; }
    Handed = std::move(*converted);
    if (Moves) {
      PublishedLocals.swap(Locals);
    } else {
      PublishedLocals.clear();
    }
    return true;
  }

  [[nodiscard]] bool SampleMaterials(double seconds, Geometry &candidate) {
    if (!Moves) { return true; }
    Motion.FactorsAt(seconds, Factors);
    for (const auto &factor : Factors) {
      if (factor.Material < 0 || factor.Material >= candidate.surfaces()) {
        Why = Says::MissingSampledMaterial;
        return false;
      }
      const MaterialInstance index(factor.Material);
      auto sampled = ApplyMaterialFactor(
          candidate.surfaceAt(index),
          factor,
          File.Materials()[static_cast<size_t>(factor.Material)].EmissiveStrength);
      if (!sampled) {
        Why = std::move(sampled.error());
        return false;
      }
      const auto published = candidate.setSurface(index, *sampled);
      if (!published) {
        Why = std::string(Says::MaterialPublicationFailed) + ": " +
              std::string(Describe(published.error()));
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] std::expected<Camera, std::string> ResolveCamera(int index) const {
    Camera placed;
    std::string why;
    const std::span<const AffineTransform> locals = PublishedLocals;
    if (!Gltf::DeclaredPlacement(File, index, placed, why, locals)) {
      return std::unexpected(std::move(why));
    }
    return placed;
  }

  [[nodiscard]] bool Wears(Geometry &candidate) {
    return Gltf::ResolveNativeMaterialImages(File, Assembled, candidate, Why);
  }
};

GltfImporter::GltfImporter() : Held_(std::make_unique<Held>()) {}

GltfImporter::~GltfImporter() = default;
GltfImporter::GltfImporter(GltfImporter &&) noexcept = default;
GltfImporter &GltfImporter::operator=(GltfImporter &&) noexcept = default;

std::expected<void, std::string> GltfImporter::load(std::string_view path) {
  auto candidate = std::make_unique<Held>();
  const auto refuse = [](std::string why) -> std::expected<void, std::string> {
    return std::unexpected(std::move(why));
  };
  if (!candidate->File.ReadFile(path)) { return refuse(candidate->File.Error()); }
  if (!candidate->Assemble(0.0)) { return refuse(std::move(candidate->Why)); }
  Held_ = std::move(candidate);
  return {};
}

std::expected<void, std::string> GltfImporter::selectMaterialVariant(std::string_view variant) {
  Held &held = *Held_;
  Gltf::VariantSelection wanted{std::string(variant)};
  int index = -1;
  if (!wanted.Against(held.File, index, held.Why)) { return std::unexpected(held.Why); }
  auto previous = std::move(held.Variant);
  held.Variant = std::move(wanted);
  if (!held.Assemble(0.0)) {
    held.Variant = std::move(previous);
    return std::unexpected(held.Why);
  }
  return {};
}

std::expected<void, std::string> GltfImporter::selectAnimations(std::span<const int> animations) {
  Held &held = *Held_;
  Gltf::Pose candidate;
  if (!animations.empty() && !Gltf::Pose::Build(held.File, animations, candidate, held.Why)) {
    return std::unexpected(held.Why);
  }
  auto previous = std::move(held.Motion);
  const bool previouslyMoved = held.Moves;
  held.Motion = std::move(candidate);
  held.Moves = held.Motion.Valid();
  if (!held.Assemble(0.0)) {
    held.Motion = std::move(previous);
    held.Moves = previouslyMoved;
    return std::unexpected(held.Why);
  }
  return {};
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
  return {};
}

int GltfImporter::cameraCount() const {
  return static_cast<int>(Held_->File.Cameras().size());
}

std::expected<Camera, std::string> GltfImporter::camera(int index) const {
  return Held_->ResolveCamera(index);
}

std::expected<Camera, GltfImporter::FrameError>
GltfImporter::frameCamera(Extent viewport) const noexcept {
  if (viewport.WidthPx <= 0 || viewport.HeightPx <= 0) {
    return std::unexpected(FrameError::InvalidViewport);
  }
  Camera fitted;
  const double aspect = static_cast<double>(viewport.WidthPx) / viewport.HeightPx;
  if (!Held_ || !Held_->Assembled.Frame(fitted, kCameraFramingFill, aspect)) {
    return std::unexpected(FrameError::InvalidBounds);
  }
  return fitted;
}

}
