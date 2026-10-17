#include "SceneResources.h"

#include "SubjectDraw.h"
#include "Surfacing.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace Says {
constexpr auto MissingPiece = "the piece handle names no live resource in this world";
constexpr auto RepeatedPiece = "an instance update names one piece more than once";
constexpr auto PieceUploadFailed = "piece geometry upload failed";
constexpr auto PieceSlotLimit = "piece storage exceeds the native slot index range";
constexpr auto PieceCapacity = "an instance update exceeds the piece capacity";
constexpr auto NoPieceMaterials = "piece material registration requires native materials";
constexpr auto PieceMaterialLimit = "piece material registration exceeds the slot index range";
constexpr auto PieceMaterialTables = "subject material tables disagree before piece registration";
}

PieceMesh SceneResources::Piece::Mesh() const noexcept {
  return {.Tangents = Source->Tangents,
          .Verts = Source->Vertices,
          .Indices = Source->Indices,
          .Clusters = Source->Clusters,
          .Colours = Source->Colours,
          .Uv1 = Source->Uv1,
          .Row = Row,
          .Instances = Rows,
          .MaxInstances = MaxInstances,
          .Surface = Surface,
          .Textured = Textured};
}

std::expected<PieceHandle, std::string>
SceneResources::PlacePiece(SubjectDraw &subjects, const PieceMesh &piece, PieceOwner owner) {
  if (!ValidPieceUv1(piece)) { return std::unexpected(std::string(kPieceUv1Error)); }
  if (FirstFreePiece_ == kNoResourceSlot && Pieces_.size() >= kNoResourceSlot) {
    return std::unexpected(Says::PieceSlotLimit);
  }
  auto source = std::make_shared<const PieceSource>(
      PieceSource{.Tangents = {piece.Tangents.begin(), piece.Tangents.end()},
                  .Vertices = {piece.Verts.begin(), piece.Verts.end()},
                  .Indices = {piece.Indices.begin(), piece.Indices.end()},
                  .Clusters = {piece.Clusters.begin(), piece.Clusters.end()},
                  .Colours = {piece.Colours.begin(), piece.Colours.end()},
                  .Uv1 = {piece.Uv1.begin(), piece.Uv1.end()}});
  Piece held{.Source = std::move(source),
             .Row = piece.Row,
             .Rows = {piece.Instances.begin(), piece.Instances.end()},
             .MaxInstances = piece.MaxInstances,
             .Surface = piece.Surface,
             .Owner = owner,
             .Textured = piece.Textured};
  if (held.Rows.empty()) { held.Rows.push_back(piece.Row); }
  std::string error;
  held.Resident = subjects.PlacePiece(held.Mesh(), error);
  if (held.Resident == kNoPiece) {
    return std::unexpected(error.empty() ? std::string(Says::PieceUploadFailed) : std::move(error));
  }
  held.State.Occupied = true;
  uint32_t slot = FirstFreePiece_;
  if (slot != kNoResourceSlot) {
    held.State.Generation = Pieces_[slot].State.Generation;
    FirstFreePiece_ = Pieces_[slot].State.NextFree;
    Pieces_[slot] = std::move(held);
  } else {
    slot = static_cast<uint32_t>(Pieces_.size());
    Pieces_.push_back(std::move(held));
  }
  return PieceHandle{.Slot = slot, .Generation = Pieces_[slot].State.Generation};
}

std::expected<size_t, std::string>
SceneResources::ReconcileStructurePieces(SubjectDraw &subjects, std::span<const PieceHandle> held) {
  std::vector<uint8_t> retained(Pieces_.size(), 0);
  for (const PieceHandle handle : held) {
    if (!HasPiece(handle) || Pieces_[handle.Slot].Owner != PieceOwner::StructureTile) {
      const Piece *current = handle.Slot < Pieces_.size() ? &Pieces_[handle.Slot] : nullptr;
      return std::unexpected(
          "a structure tile names a missing or foreign piece source (slot=" +
          std::to_string(handle.Slot) + ", generation=" + std::to_string(handle.Generation) +
          ", slots=" + std::to_string(Pieces_.size()) + ", present=" +
          std::to_string(static_cast<int>(HasPiece(handle))) + ", current generation=" +
          std::to_string(current != nullptr ? current->State.Generation : 0) +
          ", current occupied=" +
          std::to_string(static_cast<int>(current != nullptr && current->State.Occupied)) + ")");
    }
    retained[handle.Slot] = 1;
  }
  size_t released = 0;
  for (uint32_t slot = 0; slot < Pieces_.size(); ++slot) {
    const Piece &piece = Pieces_[slot];
    if (!piece.State.Occupied || piece.Owner != PieceOwner::StructureTile || retained[slot] != 0) {
      continue;
    }
    ReleasePiece(subjects, {.Slot = slot, .Generation = piece.State.Generation});
    ++released;
  }
  return released;
}

bool SceneResources::HasPiece(PieceHandle handle) const noexcept {
  return handle.Slot < Pieces_.size() && Pieces_[handle.Slot].State.Matches(handle.Generation);
}

bool SceneResources::SetPieceInstances(SubjectDraw &subjects,
                                       PieceHandle which,
                                       std::span<const Mat4> rows,
                                       std::string &error) {
  const PieceRows one{.Piece = which, .Rows = rows};
  return SetPieceInstances(subjects, {&one, 1}, error);
}

bool SceneResources::SetPieceInstances(SubjectDraw &subjects,
                                       std::span<const PieceRows> pieces,
                                       std::string &error) {
  for (size_t at = 0; at < pieces.size(); ++at) {
    const PieceRows &change = pieces[at];
    if (!HasPiece(change.Piece)) {
      error = std::string(Says::MissingPiece) + " (slot=" + std::to_string(change.Piece.Slot) +
              ", generation=" + std::to_string(change.Piece.Generation) +
              ", slots=" + std::to_string(Pieces_.size()) + ")";
      return false;
    }
    if (Pieces_[change.Piece.Slot].MaxInstances > 0 &&
        change.Rows.size() > Pieces_[change.Piece.Slot].MaxInstances) {
      error = Says::PieceCapacity;
      return false;
    }
    for (size_t earlier = 0; earlier < at; ++earlier) {
      if (pieces[earlier].Piece == change.Piece) {
        error = Says::RepeatedPiece;
        return false;
      }
    }
  }
  size_t changed = 0;
  for (; changed < pieces.size(); ++changed) {
    const PieceRows &change = pieces[changed];
    if (subjects.SetPieceInstances(Pieces_[change.Piece.Slot].Resident, change.Rows, error)) {
      continue;
    }
    while (changed > 0) {
      --changed;
      const PieceRows &undo = pieces[changed];
      std::string ignored;
      (void)subjects.SetPieceInstances(
          Pieces_[undo.Piece.Slot].Resident, Pieces_[undo.Piece.Slot].Rows, ignored);
    }
    return false;
  }
  for (const PieceRows &change : pieces) {
    Pieces_[change.Piece.Slot].Rows.assign(change.Rows.begin(), change.Rows.end());
  }
  return true;
}

void SceneResources::ReleasePiece(SubjectDraw &subjects, PieceHandle which) {
  if (!HasPiece(which)) { return; }
  Piece &piece = Pieces_[which.Slot];
  subjects.ReleasePiece(piece.Resident);
  ResourceSlotState state = piece.State;
  if (state.Release()) {
    state.NextFree = FirstFreePiece_;
    FirstFreePiece_ = which.Slot;
  }
  piece = Piece{};
  piece.State = state;
}

void SceneResources::CopySourcesFrom(const SceneResources &source, PieceSources pieces) {
  if (pieces == PieceSources::Copy) {
    Pieces_ = source.Pieces_;
    FirstFreePiece_ = source.FirstFreePiece_;
    for (Piece &piece : Pieces_) { piece.Resident = kNoPiece; }
  } else {
    Pieces_.clear();
    FirstFreePiece_ = kNoResourceSlot;
  }
  HeightPages_ = source.HeightPages_;
  FirstFreeHeightPage_ = source.FirstFreeHeightPage_;
  for (HeightPage &page : HeightPages_) { page.Resident = kNoPage; }
  GroundGrid_ = source.GroundGrid_;
  GroundReal_ = source.GroundReal_;
  GroundVirtual_ = source.GroundVirtual_;
  GroundClassification_ = source.GroundClassification_;
  PieceMaterials_ = source.PieceMaterials_;
  RegisteredPieceSlots_.clear();
}

std::expected<SceneResources::PieceMaterials, std::string>
SceneResources::ResolvePieceMaterials(Geometry source) {
  if (source.surfaces() == 0) { return std::unexpected(Says::NoPieceMaterials); }
  PieceMaterials held{.Source = std::move(source), .Slots = {}};
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

bool SceneResources::AppendPieceMaterials(SubjectDraw &subjects,
                                          SubjectDraw *glass,
                                          const PieceMaterials &materials,
                                          std::string &error) {
  if (materials.Slots.size() >= kNoSlot || RegisteredPieceSlots_.size() >= kNoSlot ||
      materials.Slots.size() > kNoSlot - RegisteredPieceSlots_.size() ||
      subjects.MaterialSlots() >= kNoSlot ||
      materials.Slots.size() > kNoSlot - subjects.MaterialSlots()) {
    error = Says::PieceMaterialLimit;
    return false;
  }
  if (!subjects.ValidateMaterials(materials.Slots, error) ||
      (glass != nullptr && !glass->ValidateMaterials(materials.Slots, error))) {
    return false;
  }
  const size_t subjectBefore = subjects.MaterialSlots();
  const size_t glassBefore = glass == nullptr ? 0 : glass->MaterialSlots();
  if (glass != nullptr && glassBefore != subjectBefore) {
    error = Says::PieceMaterialTables;
    return false;
  }
  if (!subjects.AppendMaterials(
          materials.Slots, error, SubjectResidency::ImageUploadMode::Deferred)) {
    return false;
  }
  if (glass != nullptr && !glass->AppendMaterials(materials.Slots,
                                                  error,
                                                  SubjectResidency::ImageUploadMode::Deferred)) {
    subjects.TruncateMaterials(subjectBefore);
    return false;
  }
  for (size_t at = 0; at < materials.Slots.size(); ++at) {
    RegisteredPieceSlots_.push_back(static_cast<uint32_t>(subjectBefore + at));
  }
  subjects.SetRegisteredPieceSurfaces(RegisteredPieceSlots_);
  if (glass != nullptr) { glass->SetRegisteredPieceSurfaces(RegisteredPieceSlots_); }
  return true;
}

std::expected<uint32_t, std::string>
SceneResources::RegisterPieceMaterials(SubjectDraw &subjects, SubjectDraw *glass, Geometry source) {
  auto materials = ResolvePieceMaterials(std::move(source));
  if (!materials) { return std::unexpected(std::move(materials).error()); }
  const auto first = static_cast<uint32_t>(RegisteredPieceSlots_.size());
  PieceMaterials_.reserve(PieceMaterials_.size() + 1u);
  RegisteredPieceSlots_.reserve(RegisteredPieceSlots_.size() + materials->Slots.size());
  std::string error;
  if (!AppendPieceMaterials(subjects, glass, *materials, error)) {
    return std::unexpected(std::move(error));
  }
  PieceMaterials_.push_back(std::make_shared<const PieceMaterials>(std::move(*materials)));
  return first;
}

bool SceneResources::RestorePieceMaterials(SubjectDraw &subjects,
                                           SubjectDraw *glass,
                                           std::string &error) {
  const size_t subjectBefore = subjects.MaterialSlots();
  const size_t glassBefore = glass == nullptr ? 0 : glass->MaterialSlots();
  RegisteredPieceSlots_.clear();
  for (const auto &materials : PieceMaterials_) {
    if (AppendPieceMaterials(subjects, glass, *materials, error)) { continue; }
    subjects.TruncateMaterials(subjectBefore);
    subjects.SetRegisteredPieceSurfaces({});
    if (glass != nullptr) {
      glass->TruncateMaterials(glassBefore);
      glass->SetRegisteredPieceSurfaces({});
    }
    RegisteredPieceSlots_.clear();
    return false;
  }
  return true;
}

bool SceneResources::RestorePieces(SubjectDraw &subjects, std::string &error) {
  size_t nextPiece = 0;
  auto restored = AdvancePieceRestore(subjects, nextPiece, std::numeric_limits<size_t>::max());
  if (!restored) {
    error = std::move(restored.error());
    return false;
  }
  return *restored;
}

std::expected<bool, std::string>
SceneResources::AdvancePieceRestore(SubjectDraw &subjects, size_t &nextPiece, size_t piecesMost) {
  if (piecesMost == 0) { return std::unexpected("piece restore budget is zero"); }
  size_t visited = 0;
  while (nextPiece < Pieces_.size() && visited < piecesMost) {
    Piece &piece = Pieces_[nextPiece++];
    ++visited;
    if (!piece.State.Occupied || piece.Resident != kNoPiece) { continue; }
    std::string error;
    piece.Resident = subjects.PlacePiece(piece.Mesh(), error);
    if (piece.Resident == kNoPiece) {
      return std::unexpected(error.empty() ? std::string(Says::PieceUploadFailed)
                                           : std::move(error));
    }
    if (piece.Rows.empty() && !subjects.SetPieceInstances(piece.Resident, {}, error)) {
      subjects.ReleasePiece(piece.Resident);
      piece.Resident = kNoPiece;
      return std::unexpected(std::move(error));
    }
  }
  return nextPiece == Pieces_.size();
}

size_t SceneResources::PieceSourceBytes() const noexcept {
  size_t bytes = 0;
  for (const Piece &piece : Pieces_) {
    if (piece.Source) {
      bytes += piece.Source->Tangents.capacity() * sizeof(float) +
               piece.Source->Vertices.capacity() * sizeof(StoredVertex) +
               piece.Source->Indices.capacity() * sizeof(uint32_t) +
               piece.Source->Clusters.capacity() * sizeof(DagCluster) +
               piece.Source->Colours.capacity() * sizeof(float) +
               piece.Source->Uv1.capacity() * sizeof(float);
    }
    bytes += piece.Rows.capacity() * sizeof(Mat4);
  }
  return bytes;
}

size_t SceneResources::PieceSourceCount() const noexcept {
  return static_cast<size_t>(
      std::ranges::count_if(Pieces_, [](const Piece &piece) { return piece.State.Occupied; }));
}

}
