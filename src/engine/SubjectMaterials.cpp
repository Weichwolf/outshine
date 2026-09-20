#include "SubjectMaterials.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Core {

namespace {

void OverridePart(Render::SurfaceTable &table,
                  size_t part,
                  uint32_t slot,
                  const SubjectSurfaceOverride &override,
                  std::vector<uint32_t> &users) {
  Render::SubjectMaterial material =
      override.RetainMaps ? table.Slots[slot] : Render::SubjectMaterial{};
  material.Row = override.Surface;
  if (slot < users.size() && users[slot] == 1u) {
    table.Slots[slot] = material;
    return;
  }
  if (slot < users.size()) { users[slot] -= 1u; }
  const int carried = slot < table.Material.size() ? table.Material[slot] : -1;
  const int native = slot < table.NativeMaterial.size() ? table.NativeMaterial[slot] : -1;
  table.Slots.push_back(material);
  table.Material.push_back(carried);
  table.NativeMaterial.push_back(native);
  table.Decoded.emplace_back();
  table.PartSlot[part] = static_cast<uint32_t>(table.Slots.size() - 1u);
}

size_t OverrideNativeMaterials(Render::SurfaceTable &table,
                               const Geometry &native,
                               std::span<const SubjectSurfaceOverride> overrides) {
  size_t applied = 0;
  for (size_t slot = 0; slot < table.Slots.size(); ++slot) {
    const int surface = table.NativeMaterial[slot];
    if (surface < 0 || surface >= native.surfaces()) { continue; }
    const std::string_view name = native.surfaceNameOf(surface);
    for (const SubjectSurfaceOverride &override : overrides) {
      if (override.MaterialName != name) { continue; }
      if (!override.RetainMaps) { table.Slots[slot] = Render::SubjectMaterial{}; }
      table.Slots[slot].Row = override.Surface;
      ++applied;
      break;
    }
  }
  return applied;
}

size_t OverrideNativeParts(Render::SurfaceTable &table,
                           const Render::Shape &shaped,
                           const Geometry &native,
                           std::span<const SubjectSurfaceOverride> overrides) {
  size_t applied = 0;
  std::vector<uint32_t> users(table.Slots.size(), 0u);
  for (const uint32_t slot : table.PartSlot) {
    if (slot < users.size()) { users[slot] += 1u; }
  }
  const auto nativeParts = static_cast<size_t>(native.parts());
  const size_t count = std::min(nativeParts, table.PartSlot.size());
  table.Slots.reserve(table.Slots.size() + count);
  table.Material.reserve(table.Material.size() + count);
  table.NativeMaterial.reserve(table.NativeMaterial.size() + count);
  table.Decoded.reserve(table.Decoded.size() + count);
  for (size_t part = 0; part < count; ++part) {
    const uint32_t slot = table.PartSlot[part];
    if (slot >= table.Slots.size() || part >= shaped.Parts.size()) { continue; }
    for (const SubjectSurfaceOverride &override : overrides) {
      const bool byName =
          !override.PartName.empty() && override.PartName == shaped.Parts[part].Name;
      const bool byIndex = override.PartIndex >= 0 && std::cmp_equal(override.PartIndex, part);
      if (!byName && !byIndex) { continue; }
      OverridePart(table, part, slot, override, users);
      ++applied;
      break;
    }
  }
  return applied;
}

}

void SubjectMaterials::Clear() {
  Table_ = {};
}

std::expected<void, std::string>
SubjectMaterials::Resolve(const Geometry &native,
                          const Render::Shape &shaped,
                          const Material &fallback,
                          std::span<const SubjectSurfaceOverride> overrides,
                          int groundSurface,
                          std::string_view subjectName) {
  Render::SurfaceTable candidate;
  Render::ResolveDeclaredSurface(shaped, fallback, candidate);
  candidate.NativeMaterial = candidate.Material;
  std::string error;
  if (!Render::ResolveNativeTextures(native, candidate.Slots, error)) {
    return std::unexpected(std::move(error));
  }
  const size_t applied = OverrideNativeMaterials(candidate, native, overrides) +
                         OverrideNativeParts(candidate, shaped, native, overrides);
  if (!overrides.empty() && applied == 0) {
    return std::unexpected(std::format(
        "this declaration names {} surface(s) of '{}' and the subject carries neither those "
        "material names, those part names nor those part indices -- a surface declared onto "
        "nothing changes no pixel and says it did",
        overrides.size(),
        subjectName));
  }
  if (groundSurface >= 0) {
    for (size_t slot = 0; slot < candidate.Slots.size(); ++slot) {
      if (candidate.Material[slot] == groundSurface) {
        candidate.Slots[slot].Domain = Render::SurfaceDomain::Ground;
      }
    }
  }
  Table_ = std::move(candidate);
  return {};
}

std::vector<uint32_t> SubjectMaterials::NativeSurfaceSlots(size_t surfaceCount) const {
  std::vector<uint32_t> slots(surfaceCount, Render::kNoSlot);
  for (size_t slot = 0; slot < Table_.NativeMaterial.size(); ++slot) {
    const int surface = Table_.NativeMaterial[slot];
    if (surface >= 0 && static_cast<size_t>(surface) < slots.size()) {
      slots[static_cast<size_t>(surface)] = static_cast<uint32_t>(slot);
    }
  }
  return slots;
}

}
