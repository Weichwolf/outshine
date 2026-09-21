#include "SceneResources.h"

#include "SubjectDraw.h"
#include "Surfacing.h"
#include "TerrainTileUpload.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
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
constexpr auto HeightPageLimit = "height-page storage exceeds the native slot index range";
constexpr auto HeightPageUploadFailed = "height-page upload failed";
constexpr auto NoPieceMaterials = "piece material registration requires native materials";
constexpr auto PieceMaterialLimit = "piece material registration exceeds the slot index range";
constexpr auto PieceMaterialTables = "subject material tables disagree before piece registration";
}

PieceMesh SceneResources::Piece::Mesh() const noexcept {
  return {.Tangents = Tangents,
          .Verts = Vertices,
          .Indices = Indices,
          .Clusters = Clusters,
          .Colours = Colours,
          .Row = Row,
          .Instances = Rows,
          .MaxInstances = MaxInstances,
          .Surface = Surface,
          .Textured = Textured};
}

std::expected<PieceHandle, std::string> SceneResources::PlacePiece(SubjectDraw &subjects,
                                                                   const PieceMesh &piece) {
  if (FirstFreePiece_ == kNoResourceSlot && Pieces_.size() >= kNoResourceSlot) {
    return std::unexpected(Says::PieceSlotLimit);
  }
  Piece held{.Tangents = {piece.Tangents.begin(), piece.Tangents.end()},
             .Vertices = {piece.Verts.begin(), piece.Verts.end()},
             .Indices = {piece.Indices.begin(), piece.Indices.end()},
             .Clusters = {piece.Clusters.begin(), piece.Clusters.end()},
             .Colours = {piece.Colours.begin(), piece.Colours.end()},
             .Row = piece.Row,
             .Rows = {piece.Instances.begin(), piece.Instances.end()},
             .MaxInstances = piece.MaxInstances,
             .Surface = piece.Surface,
             .Textured = piece.Textured};
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
      error = Says::MissingPiece;
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

bool SceneResources::CopySourcesFrom(const SceneResources &source, std::string &error) {
  Pieces_ = source.Pieces_;
  FirstFreePiece_ = source.FirstFreePiece_;
  for (Piece &piece : Pieces_) { piece.Resident = kNoPiece; }
  HeightPages_ = source.HeightPages_;
  FirstFreeHeightPage_ = source.FirstFreeHeightPage_;
  for (HeightPage &page : HeightPages_) { page.Resident = kNoPage; }
  GroundGrid_ = source.GroundGrid_;
  GroundReal_ = source.GroundReal_;
  GroundVirtual_ = source.GroundVirtual_;
  GroundClasses_ = source.GroundClasses_;
  GroundPalette_ = source.GroundPalette_;
  PieceMaterials_.clear();
  PieceMaterials_.reserve(source.PieceMaterials_.size());
  for (const PieceMaterials &materials : source.PieceMaterials_) {
    auto copied = ResolvePieceMaterials(materials.Source.clone());
    if (!copied) {
      error = std::move(copied).error();
      return false;
    }
    PieceMaterials_.push_back(std::move(*copied));
  }
  RegisteredPieceSlots_.clear();
  return true;
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
  if (!subjects.AppendMaterials(materials.Slots, error)) { return false; }
  if (glass != nullptr && !glass->AppendMaterials(materials.Slots, error)) {
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
  PieceMaterials_.push_back(std::move(*materials));
  return first;
}

bool SceneResources::RestorePieceMaterials(SubjectDraw &subjects,
                                           SubjectDraw *glass,
                                           std::string &error) {
  const size_t subjectBefore = subjects.MaterialSlots();
  const size_t glassBefore = glass == nullptr ? 0 : glass->MaterialSlots();
  RegisteredPieceSlots_.clear();
  for (const PieceMaterials &materials : PieceMaterials_) {
    if (AppendPieceMaterials(subjects, glass, materials, error)) { continue; }
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
  for (Piece &piece : Pieces_) {
    if (!piece.State.Occupied) { continue; }
    piece.Resident = subjects.PlacePiece(piece.Mesh(), error);
    if (piece.Resident == kNoPiece) { return false; }
  }
  return true;
}

size_t SceneResources::PieceSourceBytes() const noexcept {
  size_t bytes = 0;
  for (const Piece &piece : Pieces_) {
    bytes += piece.Tangents.capacity() * sizeof(float) +
             piece.Vertices.capacity() * sizeof(StoredVertex) +
             piece.Indices.capacity() * sizeof(uint32_t) +
             piece.Clusters.capacity() * sizeof(DagCluster) +
             piece.Colours.capacity() * sizeof(float) + piece.Rows.capacity() * sizeof(Mat4);
  }
  return bytes;
}

std::expected<HeightPageHandle, std::string>
SceneResources::PlaceHeightPage(SubjectDraw &subjects, std::span<const float> nodes) {
  if (FirstFreeHeightPage_ == kNoResourceSlot && HeightPages_.size() >= kNoResourceSlot) {
    return std::unexpected(Says::HeightPageLimit);
  }
  HeightPage held{.Nodes = {nodes.begin(), nodes.end()}};
  std::string error;
  held.Resident = subjects.Ground().PlacePage(held.Nodes, error);
  if (held.Resident == kNoPage) {
    return std::unexpected(error.empty() ? std::string(Says::HeightPageUploadFailed)
                                         : std::move(error));
  }
  held.State.Occupied = true;
  uint32_t slot = FirstFreeHeightPage_;
  if (slot != kNoResourceSlot) {
    held.State.Generation = HeightPages_[slot].State.Generation;
    FirstFreeHeightPage_ = HeightPages_[slot].State.NextFree;
    HeightPages_[slot] = std::move(held);
  } else {
    slot = static_cast<uint32_t>(HeightPages_.size());
    HeightPages_.push_back(std::move(held));
  }
  return HeightPageHandle{.Slot = slot, .Generation = HeightPages_[slot].State.Generation};
}

bool SceneResources::HasHeightPage(HeightPageHandle handle) const noexcept {
  return handle.Slot < HeightPages_.size() &&
         HeightPages_[handle.Slot].State.Matches(handle.Generation);
}

void SceneResources::ReleaseHeightPage(SubjectDraw &subjects, HeightPageHandle which) {
  if (!HasHeightPage(which)) { return; }
  HeightPage &page = HeightPages_[which.Slot];
  subjects.Ground().ReleasePage(page.Resident);
  ResourceSlotState state = page.State;
  if (state.Release()) {
    state.NextFree = FirstFreeHeightPage_;
    FirstFreeHeightPage_ = which.Slot;
  }
  page = HeightPage{};
  page.State = state;
}

PageId SceneResources::HeightPageResident(HeightPageHandle which) const noexcept {
  return HasHeightPage(which) ? HeightPages_[which.Slot].Resident : kNoPage;
}

bool SceneResources::RestoreHeightPages(SubjectDraw &subjects, std::string &error) {
  for (HeightPage &page : HeightPages_) {
    if (!page.State.Occupied) { continue; }
    page.Resident = subjects.Ground().PlacePage(page.Nodes, error);
    if (page.Resident == kNoPage) { return false; }
  }
  return true;
}

bool SceneResources::SetGroundGrid(SubjectDraw &subjects,
                                   std::span<const float> fractions,
                                   std::string &error) {
  if (!subjects.Ground().SetGrid(fractions, error)) { return false; }
  GroundGrid_.assign(fractions.begin(), fractions.end());
  return true;
}

bool SceneResources::SetTerrainTiles(SubjectDraw &subjects,
                                     std::span<const TerrainTile> real,
                                     std::span<const TerrainTile> virtual_,
                                     std::string &error) {
  const auto translate = [this, &error](std::span<const TerrainTile> source,
                                        std::vector<GroundTile> &into) {
    into.reserve(source.size());
    for (const TerrainTile &tile : source) {
      const auto encoded = EncodeTerrainTile(tile, HeightPageResident(tile.Page));
      if (!encoded) {
        error = std::string(encoded.error()) + ": slot=" + std::to_string(tile.Page.Slot) +
                ", generation=" + std::to_string(tile.Page.Generation);
        return false;
      }
      into.push_back(*encoded);
    }
    return true;
  };
  std::vector<GroundTile> residentReal;
  std::vector<GroundTile> residentVirtual;
  if (!translate(real, residentReal) || !translate(virtual_, residentVirtual) ||
      !subjects.Ground().SetInstances(residentReal, residentVirtual, error)) {
    return false;
  }
  GroundReal_.assign(real.begin(), real.end());
  GroundVirtual_.assign(virtual_.begin(), virtual_.end());
  return true;
}

bool SceneResources::RestoreTerrain(SubjectDraw &subjects, std::string &error) {
  if (!RestoreHeightPages(subjects, error)) { return false; }
  if (!GroundGrid_.empty() && !subjects.Ground().SetGrid(GroundGrid_, error)) { return false; }
  return SetTerrainTiles(subjects, GroundReal_, GroundVirtual_, error);
}

void SceneResources::SetGroundClassification(std::span<const uint32_t> classes,
                                             std::span<const float> palette) {
  GroundClasses_.assign(classes.begin(), classes.end());
  GroundPalette_.assign(palette.begin(), palette.end());
}

size_t SceneResources::HeightPageSourceBytes() const noexcept {
  size_t bytes = 0;
  for (const HeightPage &page : HeightPages_) { bytes += page.Nodes.capacity() * sizeof(float); }
  return bytes;
}

}
