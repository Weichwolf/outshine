#include "SubjectDraw.h"
#include "GpuPlacement.h"
#include "SubjectResidency.h"
#include "math/Mat4.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Render {

void SubjectDraw::BindPlacementStorage(const PassRecording &into, VertexLayout layout) {
  std::array<SDL_GPUBuffer *, 3> rows{Bound().Buffer(SubjectResidency::Stream::Placements).Get()};
  uint32_t count = 1;
  if (CarriesColour(layout)) {
    rows[count++] = Bound().Buffer(SubjectResidency::Stream::Colour).Get();
  }
  if (CarriesTangent(layout)) {
    rows[count++] = Bound().Buffer(SubjectResidency::Stream::Tangent).Get();
  }
  SDL_BindGPUVertexStorageBuffers(into.Pass, 0, rows.data(), count);
}

bool SubjectDraw::HandPlacements(bool deferred, std::string &error) {
  const size_t needed = std::max(Placed_.size() / 16u, static_cast<size_t>(SubjectRows_));
  size_t all = needed;
  for (const Piece &piece : Pieces_) { all += piece.Rows.size(); }
  if (!RowsStale_ && Rows_.size() == all) { return true; }
  RowsStale_ = false;
  if (all == 0) { return true; }
  Rows_.assign(all, {});
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
    const bool carried = placed ? row < Stamped_.size() && Stamped_[row] != 0u : ModelStamp_ != 0u;
    const double *const carriedFrom = placed ? Before_.data() + row * 16u : ModelBefore_.data();
    const double *const was = carried ? carriedFrom : now;
    for (size_t at = 0; at < 16u; ++at) {
      Rows_[row].Current[at] = static_cast<float>(now[at]);
      Rows_[row].Previous[at] = static_cast<float>(was[at]);
    }
    Rows_[row].ColourOffset = Bound().SubjectColours().First - Bound().SubjectVertices().First;
    Rows_[row].TangentOffset = Bound().SubjectTangents().First - Bound().SubjectVertices().First;
  }
  std::array rows = {SubjectResidency::Crossing{
      .Which = SubjectResidency::Stream::Placements,
      .Usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
      .From = Rows_.data(),
      .Bytes = static_cast<uint32_t>(Rows_.size() * sizeof(GpuPlacement))}};
  return Bound().Cross(rows, deferred, error);
}

}
