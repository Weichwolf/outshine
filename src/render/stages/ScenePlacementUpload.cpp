#include "GeometryRenderer.h"
#include "GpuPlacement.h"
#include "GeometryResidency.h"
#include "Mat4.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Render {

bool GeometryRenderer::HandPlacements(bool deferred, std::string &error) {
  const size_t needed = std::max(Placed_.size() / 16u, static_cast<size_t>(MainMeshRows_));
  size_t all = needed;
  for (const Piece &piece : Pieces_) { all += piece.Rows.size(); }
  if (!RowsStale_ && Rows_.size() == all) {
    PrepareShadowCasters();
    return true;
  }
  ShadowCasters_.Invalidate();
  if (all == 0) { return true; }
  Rows_.assign(all, {});
  bool rowsHaveMotion = false;
  size_t offset = needed;
  for (const Piece &piece : Pieces_) {
    for (const Mat4 &row : piece.Rows) {
      for (size_t at = 0; at < 16u; ++at) {
        const auto held = static_cast<float>(row[at]);
        Rows_[offset].Current[at] = held;
        Rows_[offset].Previous[at] = held;
      }
      Rows_[offset].ColourOffset = piece.C.First - piece.V.First;
      Rows_[offset].TangentOffset = piece.T.First - piece.V.First;
      ++offset;
    }
  }
  for (size_t row = 0; row < needed; ++row) {
    const bool placed = row * 16u + 16u <= Placed_.size();
    const double *const now = placed ? Placed_.data() + row * 16u : Model.data();
    const bool carried =
        placed ? row < Stamped_.size() && Stamped_[row] == Frame_ : ModelStamp_ == Frame_;
    const double *const carriedFrom = placed ? Before_.data() + row * 16u : ModelBefore_.data();
    const double *const was = carried ? carriedFrom : now;
    for (size_t at = 0; at < 16u; ++at) {
      Rows_[row].Current[at] = static_cast<float>(now[at]);
      Rows_[row].Previous[at] = static_cast<float>(was[at]);
    }
    Rows_[row].ColourOffset = Bound().MainColourRange().First - Bound().MainVertexRange().First;
    Rows_[row].TangentOffset = Bound().MainTangentRange().First - Bound().MainVertexRange().First;
    rowsHaveMotion =
        rowsHaveMotion || (Binding().WritesVelocity && Rows_[row].Current != Rows_[row].Previous);
  }
  std::array rows = {GeometryResidency::Crossing{
      .Which = GeometryResidency::Stream::Placements,
      .Usage = Native::BufferUse::Storage,
      .From = Rows_.data(),
      .Bytes = static_cast<uint32_t>(Rows_.size() * sizeof(GpuPlacement))}};
  const auto accepted = Bound().Cross(rows, deferred);
  if (!accepted) {
    error = accepted.error().Message;
    return false;
  }
  RowsStale_ = false;
  RowsHaveMotion_ = rowsHaveMotion;
  PrepareShadowCasters();
  return true;
}

void GeometryRenderer::CarryFrame() {
  ++Frame_;
  RowsStale_ = RowsStale_ || RowsHaveMotion_;
  RowsHaveMotion_ = false;
}

void GeometryRenderer::PrepareShadowCasters() {
  if (ShadowCasters_.Ready()) { return; }
  ShadowCasters_.Prepare(
      Batches, TableTemplates_, TableSpheres_, Rows_, MainMeshRows_, MainMeshBoundsCurrent_);
}

}
