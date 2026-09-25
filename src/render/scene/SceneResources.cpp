#include "SceneResources.h"

#include "SubjectDraw.h"
#include "Surfacing.h"
#include "TerrainTileUpload.h"

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
constexpr auto HeightPageLimit = "height-page storage exceeds the native slot index range";
constexpr auto HeightPageUploadFailed = "height-page upload failed";
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
          .Row = Row,
          .Instances = Rows,
          .MaxInstances = MaxInstances,
          .Surface = Surface,
          .Textured = Textured};
}

std::expected<PieceHandle, std::string>
SceneResources::PlacePiece(SubjectDraw &subjects, const PieceMesh &piece, PieceOwner owner) {
  if (FirstFreePiece_ == kNoResourceSlot && Pieces_.size() >= kNoResourceSlot) {
    return std::unexpected(Says::PieceSlotLimit);
  }
  auto source = std::make_shared<const PieceSource>(
      PieceSource{.Tangents = {piece.Tangents.begin(), piece.Tangents.end()},
                  .Vertices = {piece.Verts.begin(), piece.Verts.end()},
                  .Indices = {piece.Indices.begin(), piece.Indices.end()},
                  .Clusters = {piece.Clusters.begin(), piece.Clusters.end()},
                  .Colours = {piece.Colours.begin(), piece.Colours.end()}});
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

bool SceneResources::CopySourcesFrom(const SceneResources &source,
                                     PieceSources pieces,
                                     std::string &error) {
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
               piece.Source->Colours.capacity() * sizeof(float);
    }
    bytes += piece.Rows.capacity() * sizeof(Mat4);
  }
  return bytes;
}

size_t SceneResources::PieceSourceCount() const noexcept {
  return static_cast<size_t>(
      std::ranges::count_if(Pieces_, [](const Piece &piece) { return piece.State.Occupied; }));
}

std::expected<HeightPageHandle, std::string>
SceneResources::PlaceHeightPage(SubjectDraw &subjects, std::span<const float> nodes) {
  if (FirstFreeHeightPage_ == kNoResourceSlot && HeightPages_.size() >= kNoResourceSlot) {
    return std::unexpected(Says::HeightPageLimit);
  }
  HeightPage held{.Nodes = std::make_shared<const std::vector<float>>(nodes.begin(), nodes.end())};
  std::string error;
  held.Resident = subjects.Ground().PlacePage(*held.Nodes, error);
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
    page.Resident = subjects.Ground().PlacePage(*page.Nodes, error);
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
  size_t nextPage = 0;
  auto restored = AdvanceTerrainRestore(subjects, nextPage, std::numeric_limits<size_t>::max());
  if (!restored) {
    error = std::move(restored.error());
    return false;
  }
  return *restored;
}

std::expected<bool, std::string>
SceneResources::AdvanceTerrainRestore(SubjectDraw &subjects, size_t &nextPage, size_t pagesMost) {
  assert(nextPage <= HeightPages_.size());
  const size_t count = std::min(pagesMost, HeightPages_.size() - nextPage);
  const size_t end = nextPage + count;
  for (; nextPage < end; ++nextPage) {
    HeightPage &page = HeightPages_[nextPage];
    if (!page.State.Occupied) { continue; }
    std::string error;
    page.Resident = subjects.Ground().PlacePage(*page.Nodes, error);
    if (page.Resident == kNoPage) { return std::unexpected(std::move(error)); }
  }
  if (nextPage < HeightPages_.size()) { return false; }
  std::string error;
  if (!GroundGrid_.empty() && !subjects.Ground().SetGrid(GroundGrid_, error)) {
    return std::unexpected(std::move(error));
  }
  if (!SetTerrainTiles(subjects, GroundReal_, GroundVirtual_, error)) {
    return std::unexpected(std::move(error));
  }
  return true;
}

void SceneResources::SetGroundClassification(std::span<const uint32_t> classes,
                                             std::span<const float> palette) {
  const auto classCopy =
      std::make_shared<const std::vector<uint32_t>>(classes.begin(), classes.end());
  const auto paletteCopy =
      std::make_shared<const std::vector<float>>(palette.begin(), palette.end());
  SetGroundClassification({.Classes = {classCopy, classCopy->data()},
                           .ClassWords = classCopy->size(),
                           .Palette = {paletteCopy, paletteCopy->data()},
                           .PaletteFloats = paletteCopy->size()});
}

void SceneResources::SetGroundClassification(GroundClassificationSource source) noexcept {
  GroundClassification_ = std::move(source);
}

size_t SceneResources::HeightPageSourceBytes() const noexcept {
  size_t bytes = 0;
  for (const HeightPage &page : HeightPages_) {
    if (page.Nodes) { bytes += page.Nodes->capacity() * sizeof(float); }
  }
  return bytes;
}

}
