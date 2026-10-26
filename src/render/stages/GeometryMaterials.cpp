#include "GeometryRenderer.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr Winding kGeometryWinding = Winding::Trusted;

const char *KindName(SurfaceKind kind) {
  switch (kind) {
    case SurfaceKind::Opaque: return "OPAQUE";
    case SurfaceKind::Masked: return "MASK";
    case SurfaceKind::Blended: return "BLEND";
    case SurfaceKind::ThinTransmissive: return "a thin transmissive sheet";
    case SurfaceKind::Refractive: return "a refracting volume";
  }
  return "an undeclared surface";
}
}

bool GeometryRenderer::BindSurface(const MaterialBinding &material,
                                   std::string &error,
                                   GeometryResidency::ImageUploadMode mode) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kGeometryWinding);
  slot.Domain = material.Domain;
  if (material.Row.Unlit) {
    slot.Unlit = {
        {material.Row.BaseColour[0], material.Row.BaseColour[1], material.Row.BaseColour[2]}};
  }
  slot.ReadsSecondUv = material.ReadsSecondUv();
  const auto bind = [&](const MaterialTexture &texture,
                        GeometryResidency::Transfer transfer,
                        TexelKind texel,
                        GeometryResidency::BoundImage &into) {
    auto uploaded = Bound().Upload(texture, transfer, texel, mode);
    if (!uploaded) {
      error = std::move(uploaded.error().Message);
      return false;
    }
    into = std::move(*uploaded);
    return true;
  };
  if (!bind(material.Colour, GeometryResidency::Transfer::Srgb, TexelKind::Value, slot.Colour) ||
      !bind(material.Normal,
            GeometryResidency::Transfer::Linear,
            TexelKind::Direction,
            slot.Normal) ||
      !bind(material.MetalRough,
            GeometryResidency::Transfer::Linear,
            TexelKind::Value,
            slot.MetalRough) ||
      !bind(
          material.Emissive, GeometryResidency::Transfer::Srgb, TexelKind::Value, slot.Emissive) ||
      !bind(material.SpecularStrength,
            GeometryResidency::Transfer::Linear,
            TexelKind::Value,
            slot.SpecularStrength) ||
      !bind(material.SpecularTint,
            GeometryResidency::Transfer::Srgb,
            TexelKind::Value,
            slot.SpecularTint) ||
      !bind(material.DiffuseReflection,
            GeometryResidency::Transfer::Linear,
            TexelKind::Value,
            slot.DiffuseReflection) ||
      !bind(material.DiffuseTransmission,
            GeometryResidency::Transfer::Linear,
            TexelKind::Value,
            slot.DiffuseTransmission)) {
    return false;
  }

  const auto identity = static_cast<float>(Slots.size() + 1u);
  slot.Row = PackMaterial(material, identity);
  Slots.push_back(std::move(slot));
  return true;
}

uint32_t GeometryRenderer::ColourImages() const {
  std::vector<const Native::Texture *> distinct;
  for (const SurfaceSlot &slot : Slots) {
    const Native::Texture *const colour = slot.Colour.Image.get();
    bool seen = false;
    for (const Native::Texture *const held : distinct) { seen = seen || held == colour; }
    if (!seen) { distinct.push_back(colour); }
  }
  return static_cast<uint32_t>(distinct.size());
}

uint32_t GeometryRenderer::Layouts() const {
  std::vector<VertexLayout> distinct;
  for (const DrawBatch &batch : Batches) {
    bool seen = false;
    for (const VertexLayout held : distinct) { seen = seen || held == batch.Layout; }
    if (!seen) { distinct.push_back(batch.Layout); }
  }
  return static_cast<uint32_t>(distinct.size());
}

uint32_t GeometryRenderer::Textured() const {
  uint32_t wearing = 0;
  for (const SurfaceSlot &slot : Slots) {
    if (slot.Colour.Image != nullptr) { ++wearing; }
  }
  return wearing;
}

bool GeometryRenderer::ValidateMaterials(std::span<const MaterialBinding> materials,
                                         std::string &error) const {
  if (Binding().Device == nullptr) {
    error = "the geometry unit has no device, so no surface can be bound";
    return false;
  }
  for (size_t slot = 0; slot < materials.size(); ++slot) {
    const SurfaceKind kind = materials[slot].State().Kind();

    if ((kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive) &&
        Binding().Behind == nullptr && !Binding().GlassDrawnElsewhere) {
      error = "surface slot " + std::to_string(slot) + " is " + KindName(kind) +
              ", and no pass of this plan draws it -- what is transmitted through a sheet or "
              "refracted by a volume is the scene behind it, so a geometry carrying one needs the "
              "transmissive pass declared, and drawing it opaque instead would be a picture nobody "
              "asked for";
      return false;
    }
  }
  return true;
}

bool GeometryRenderer::AppendMaterials(std::span<const MaterialBinding> materials,
                                       std::string &error,
                                       GeometryResidency::ImageUploadMode mode) {
  if (!ValidateMaterials(materials, error)) { return false; }
  const size_t was = Slots.size();
  for (const auto &material : materials) {
    if (BindSurface(material, error, mode)) { continue; }
    Slots.resize(was);
    return false;
  }
  if (!materials.empty()) { TablesStale_ = true; }
  return true;
}

void GeometryRenderer::TruncateMaterials(size_t count) noexcept {
  if (count >= Slots.size()) { return; }
  if (TruncatesShadowMaterials(count)) { ShadowChanges_.Unknown(); }
  Slots.resize(count);
  TablesStale_ = true;
}

bool GeometryRenderer::SetMaterials(std::span<const MaterialBinding> materials,
                                    std::string &error) {
  if (!ValidateMaterials(materials, error)) { return false; }
  ShadowChanges_.Unknown();
  auto previousSlots = std::move(Slots);
  auto previousBatches = std::move(Batches);
  auto previousLayouts = std::move(BatchLayout);
  const uint32_t previousIndices = Bound().Shape().Indices;
  Slots.clear();
  Batches.clear();
  BatchLayout.clear();
  Bound().Shape().Indices = 0;
  if (AppendMaterials(materials, error)) { return true; }
  Slots = std::move(previousSlots);
  Batches = std::move(previousBatches);
  BatchLayout = std::move(previousLayouts);
  Bound().Shape().Indices = previousIndices;
  return false;
}

}
