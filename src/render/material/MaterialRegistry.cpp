#include "MaterialRegistry.h"
#include "MaterialSource.h"
#include "MaterialResolution.h"
#include "GeometryRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
std::expected<MaterialSource, std::string> MaterialRegistry::Resolve(Geometry source) {
  if (source.surfaces() == 0) { return std::unexpected("material registration requires surfaces"); }
  MaterialSource held{
      .Source = std::move(source), .Slots = {}, .RegisteredRows = {}, .Lifetime = {}};
  held.Slots.resize(static_cast<size_t>(held.Source.surfaces()));
  for (size_t at = 0; at < held.Slots.size(); ++at) {
    held.Slots[at].Row = held.Source.surfaceAt(MaterialInstance(static_cast<int>(at)));
  }
  std::string error;
  if (!ResolveNativeTextures(held.Source, held.Slots, error)) {
    return std::unexpected(std::move(error));
  }
  return held;
}

uint32_t MaterialRegistry::TakeRow() {
  if (FirstFreeRow_ != kNoResourceSlot) {
    const uint32_t result = FirstFreeRow_;
    FirstFreeRow_ = Rows_[result].NextFree;
    Rows_[result].NextFree = kNoResourceSlot;
    return result;
  }
  Rows_.emplace_back();
  return static_cast<uint32_t>(Rows_.size() - 1);
}

void MaterialRegistry::GiveRow(uint32_t which) {
  Rows_[which].Owner.reset();
  Rows_[which].NextFree = FirstFreeRow_;
  FirstFreeRow_ = which;
}

bool MaterialRegistry::Contains(const MaterialRegistration &material) const noexcept {
  return material.Source && material.First < Rows_.size() &&
         Rows_[material.First].Owner.lock() == material.Source;
}

void MaterialRegistry::Retire(uint32_t which, GeometryRenderer *geometry, GeometryRenderer *glass) {
  auto &record = Sources_[which];
  for (const uint32_t row : record.Rows) {
    if (geometry != nullptr) {
      geometry->ClearMaterialRow(geometry->NativeMaterialSlots() + row);
      geometry->SetRegisteredPieceSurface(row, kNoMaterialSlot);
    }
    if (glass != nullptr) {
      glass->ClearMaterialRow(glass->NativeMaterialSlots() + row);
      glass->SetRegisteredPieceSurface(row, kNoMaterialSlot);
    }
    GiveRow(row);
  }
  record.Source.reset();
  std::vector<uint32_t>().swap(record.Rows);
  if (record.State.Release()) {
    record.State.NextFree = FirstFreeSource_;
    FirstFreeSource_ = which;
  }
}

void MaterialRegistry::Collect(GeometryRenderer &geometry, GeometryRenderer *glass) {
  Retirements_->Take(Retired_);
  for (const auto event : Retired_) {
    if (event.Slot >= Sources_.size() || !Sources_[event.Slot].State.Matches(event.Generation)) {
      continue;
    }
    Retire(event.Slot, Binding_ ? &geometry : nullptr, Binding_ ? glass : nullptr);
  }
  Retired_.clear();
}

bool MaterialRegistry::Prepare(GeometryRenderer &geometry,
                               GeometryRenderer *glass,
                               std::string &error) {
  if (!Binding_ || Binding_->Geometry != &geometry || Binding_->Glass != glass ||
      Binding_->GeometryRevision != geometry.MaterialRevision() ||
      (glass != nullptr && Binding_->GlassRevision != glass->MaterialRevision())) {
    return Restore(geometry, glass, error);
  }
  Collect(geometry, glass);
  return true;
}

bool MaterialRegistry::Write(GeometryRenderer &geometry,
                             GeometryRenderer *glass,
                             const MaterialSource &source,
                             std::string &error) {
  const auto write = [&](GeometryRenderer &into) {
    if (into.NativeMaterialSlots() >= kNoMaterialSlot ||
        Rows_.size() > kNoMaterialSlot - into.NativeMaterialSlots()) {
      error = "registered material rows exceed native indices";
      return false;
    }
    std::vector<uint32_t> physical;
    physical.reserve(source.RegisteredRows.size());
    for (const uint32_t row : source.RegisteredRows) {
      physical.push_back(static_cast<uint32_t>(into.NativeMaterialSlots() + row));
    }
    return into.WriteMaterialRows(source.Slots, physical, error);
  };
  if (!write(geometry)) { return false; }
  if (glass != nullptr && !write(*glass)) {
    for (const uint32_t row : source.RegisteredRows) {
      geometry.ClearMaterialRow(geometry.NativeMaterialSlots() + row);
    }
    return false;
  }
  for (const uint32_t row : source.RegisteredRows) {
    geometry.SetRegisteredPieceSurface(row,
                                       static_cast<uint32_t>(geometry.NativeMaterialSlots() + row));
    if (glass != nullptr) {
      glass->SetRegisteredPieceSurface(row,
                                       static_cast<uint32_t>(glass->NativeMaterialSlots() + row));
    }
  }
  return true;
}

std::expected<MaterialRegistration, std::string>
MaterialRegistry::Register(GeometryRenderer &geometry, GeometryRenderer *glass, Geometry source) {
  std::string error;
  if (!Prepare(geometry, glass, error)) { return std::unexpected(std::move(error)); }
  auto materials = Resolve(std::move(source));
  if (!materials) { return std::unexpected(std::move(materials).error()); }
  if (materials->Slots.size() >= kNoMaterialSlot ||
      materials->Slots.size() > kNoMaterialSlot - Rows_.size() ||
      (FirstFreeSource_ == kNoResourceSlot && Sources_.size() >= kNoResourceSlot)) {
    return std::unexpected("material registration exceeds slot indices");
  }
  if (!geometry.ValidateMaterials(materials->Slots, error) ||
      (glass != nullptr && !glass->ValidateMaterials(materials->Slots, error))) {
    return std::unexpected(std::move(error));
  }
  materials->RegisteredRows.reserve(materials->Slots.size());
  for (size_t at = 0; at < materials->Slots.size(); ++at) {
    materials->RegisteredRows.push_back(TakeRow());
  }
  if (!Write(geometry, glass, *materials, error)) {
    for (const uint32_t row : materials->RegisteredRows) { GiveRow(row); }
    return std::unexpected(std::move(error));
  }
  uint32_t slot = FirstFreeSource_;
  if (slot == kNoResourceSlot) {
    slot = static_cast<uint32_t>(Sources_.size());
    Sources_.emplace_back();
  } else {
    FirstFreeSource_ = Sources_[slot].State.NextFree;
  }
  auto &record = Sources_[slot];
  record.State.Occupied = true;
  record.State.NextFree = kNoResourceSlot;
  materials->Lifetime = std::make_shared<MaterialLifetime>();
  auto owner = std::make_shared<const MaterialSource>(std::move(*materials));
  record.Source = owner;
  record.Rows = owner->RegisteredRows;
  for (const uint32_t row : record.Rows) { Rows_[row].Owner = owner; }
  owner->Lifetime->Subscribe(Retirements_, {.Slot = slot, .Generation = record.State.Generation});
  return MaterialRegistration{.First = record.Rows.front(), .Source = std::move(owner)};
}

bool MaterialRegistry::Restore(GeometryRenderer &geometry,
                               GeometryRenderer *glass,
                               std::string &error) {
  Binding_.reset();
  Collect(geometry, glass);
  geometry.TruncateMaterials(geometry.NativeMaterialSlots());
  if (glass != nullptr) { glass->TruncateMaterials(glass->NativeMaterialSlots()); }
  const std::vector<uint32_t> empty(Rows_.size(), kNoMaterialSlot);
  geometry.SetRegisteredPieceSurfaces(empty);
  if (glass != nullptr) { glass->SetRegisteredPieceSurfaces(empty); }
  for (size_t at = 0; at < Sources_.size(); ++at) {
    const auto &record = Sources_[at];
    if (!record.State.Occupied) { continue; }
    const auto owner = record.Source.lock();
    if (!owner) {
      Retire(static_cast<uint32_t>(at), nullptr, nullptr);
      continue;
    }
    if (!Write(geometry, glass, *owner, error)) {
      geometry.TruncateMaterials(geometry.NativeMaterialSlots());
      geometry.SetRegisteredPieceSurfaces({});
      if (glass != nullptr) {
        glass->TruncateMaterials(glass->NativeMaterialSlots());
        glass->SetRegisteredPieceSurfaces({});
      }
      return false;
    }
    owner->Lifetime->Subscribe(
        Retirements_, {.Slot = static_cast<uint32_t>(at), .Generation = record.State.Generation});
  }
  Binding_ = Binding{.Geometry = &geometry,
                     .Glass = glass,
                     .GeometryRevision = geometry.MaterialRevision(),
                     .GlassRevision = glass == nullptr ? 0 : glass->MaterialRevision()};
  return true;
}

void MaterialRegistry::CopySourcesFrom(const MaterialRegistry &source) {
  Rows_ = source.Rows_;
  FirstFreeRow_ = source.FirstFreeRow_;
  Sources_ = source.Sources_;
  FirstFreeSource_ = source.FirstFreeSource_;
  Retirements_ = std::make_shared<MaterialRetirementQueue>();
  Retired_.clear();
  Binding_.reset();
}
}
