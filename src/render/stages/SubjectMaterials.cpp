#include "SubjectDraw.h"
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
constexpr Winding kSubjectWinding = Winding::Trusted;

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

bool SubjectDraw::BindSurface(const SubjectMaterial &material,
                              std::string &error,
                              SubjectResidency::ImageUploadMode mode) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kSubjectWinding);
  slot.Domain = material.Domain;
  if (material.Row.Unlit) {
    slot.Unlit = {
        {material.Row.BaseColour[0], material.Row.BaseColour[1], material.Row.BaseColour[2]}};
  }
  slot.ReadsSecondUv = material.ReadsSecondUv();
  const auto bind = [&](const SubjectTexture &texture,
                        SubjectResidency::Transfer transfer,
                        TexelKind texel,
                        SubjectResidency::BoundImage &into) {
    auto uploaded = Bound().Upload(texture, transfer, texel, mode);
    if (!uploaded) {
      error = std::move(uploaded.error().Message);
      return false;
    }
    into = std::move(*uploaded);
    return true;
  };
  if (!bind(material.Colour, SubjectResidency::Transfer::Srgb, TexelKind::Value, slot.Colour) ||
      !bind(
          material.Normal, SubjectResidency::Transfer::Linear, TexelKind::Direction, slot.Normal) ||
      !bind(material.MetalRough,
            SubjectResidency::Transfer::Linear,
            TexelKind::Value,
            slot.MetalRough) ||
      !bind(material.Emissive, SubjectResidency::Transfer::Srgb, TexelKind::Value, slot.Emissive) ||
      !bind(material.SpecularStrength,
            SubjectResidency::Transfer::Linear,
            TexelKind::Value,
            slot.SpecularStrength) ||
      !bind(material.SpecularTint,
            SubjectResidency::Transfer::Srgb,
            TexelKind::Value,
            slot.SpecularTint) ||
      !bind(material.DiffuseReflection,
            SubjectResidency::Transfer::Linear,
            TexelKind::Value,
            slot.DiffuseReflection) ||
      !bind(material.DiffuseTransmission,
            SubjectResidency::Transfer::Linear,
            TexelKind::Value,
            slot.DiffuseTransmission)) {
    return false;
  }

  const auto identity = static_cast<float>(Slots.size() + 1u);
  slot.Row = PackSubjectMaterial(material, identity);
  Slots.push_back(std::move(slot));
  return true;
}

uint32_t SubjectDraw::ColourImages() const {
  std::vector<const Native::Texture *> distinct;
  for (const SurfaceSlot &slot : Slots) {
    const Native::Texture *const colour = slot.Colour.Image.get();
    bool seen = false;
    for (const Native::Texture *const held : distinct) { seen = seen || held == colour; }
    if (!seen) { distinct.push_back(colour); }
  }
  return static_cast<uint32_t>(distinct.size());
}

uint32_t SubjectDraw::Layouts() const {
  std::vector<VertexLayout> distinct;
  for (const DrawBatch &batch : Batches) {
    bool seen = false;
    for (const VertexLayout held : distinct) { seen = seen || held == batch.Layout; }
    if (!seen) { distinct.push_back(batch.Layout); }
  }
  return static_cast<uint32_t>(distinct.size());
}

uint32_t SubjectDraw::DistinctPlacements() const {
  const size_t rows = Placed_.size() / 16u;
  if (rows == 0) { return 0; }
  uint32_t distinct = 0;
  for (size_t row = 0; row < rows; ++row) {
    bool seen = false;
    for (size_t over = 0; over < row && !seen; ++over) {
      seen = std::ranges::equal(std::span(Placed_).subspan(row * 16u, 16u),
                                std::span(Placed_).subspan(over * 16u, 16u));
    }
    if (!seen) { ++distinct; }
  }
  return distinct;
}

uint32_t SubjectDraw::Textured() const {
  uint32_t wearing = 0;
  for (const SurfaceSlot &slot : Slots) {
    if (slot.Colour.Image != nullptr) { ++wearing; }
  }
  return wearing;
}

bool SubjectDraw::ValidateMaterials(std::span<const SubjectMaterial> materials,
                                    std::string &error) const {
  if (Binding().Device == nullptr) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  for (size_t slot = 0; slot < materials.size(); ++slot) {
    const SurfaceKind kind = materials[slot].State().Kind();

    if ((kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive) &&
        Binding().Behind == nullptr && !Binding().GlassDrawnElsewhere) {
      error = "surface slot " + std::to_string(slot) + " is " + KindName(kind) +
              ", and no pass of this plan draws it -- what is transmitted through a sheet or "
              "refracted by a volume is the scene behind it, so a subject carrying one needs the "
              "transmissive pass declared, and drawing it opaque instead would be a picture nobody "
              "asked for";
      return false;
    }
  }
  return true;
}

bool SubjectDraw::AppendMaterials(std::span<const SubjectMaterial> materials,
                                  std::string &error,
                                  SubjectResidency::ImageUploadMode mode) {
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

void SubjectDraw::TruncateMaterials(size_t count) noexcept {
  if (count >= Slots.size()) { return; }
  Slots.resize(count);
  TablesStale_ = true;
}

bool SubjectDraw::SetMaterials(std::span<const SubjectMaterial> materials, std::string &error) {
  if (!ValidateMaterials(materials, error)) { return false; }
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
