#include "SceneResources.h"

#include "SubjectDraw.h"
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

void SceneResources::CopySourcesFrom(const SceneResources &source) {
  Pieces_ = source.Pieces_;
  FirstFreePiece_ = source.FirstFreePiece_;
  for (Piece &piece : Pieces_) { piece.Resident = kNoPiece; }
  HeightPages_ = source.HeightPages_;
  FirstFreeHeightPage_ = source.FirstFreeHeightPage_;
  for (HeightPage &page : HeightPages_) { page.Resident = kNoPage; }
  GroundGrid_ = source.GroundGrid_;
  GroundReal_ = source.GroundReal_;
  GroundVirtual_ = source.GroundVirtual_;
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
        error = encoded.error();
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

size_t SceneResources::HeightPageSourceBytes() const noexcept {
  size_t bytes = 0;
  for (const HeightPage &page : HeightPages_) { bytes += page.Nodes.capacity() * sizeof(float); }
  return bytes;
}

}
