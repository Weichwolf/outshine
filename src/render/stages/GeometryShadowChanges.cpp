#include "GeometryRenderer.h"
#include "CasterChanges.h"
#include "Box.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace outshine::Render {
void GeometryRenderer::RecordPieceShadowChange(const std::optional<std::array<float, 4>> &before,
                                               size_t beforeRows,
                                               const std::optional<std::array<float, 4>> &after,
                                               size_t afterRows) noexcept {
  if (!PiecesVisible_ || (beforeRows == 0 && afterRows == 0)) { return; }
  const auto oldBounds =
      beforeRows != 0 && before ? CasterChanges::SphereBounds(*before) : std::optional<Boxf>{};
  const auto newBounds =
      afterRows != 0 && after ? CasterChanges::SphereBounds(*after) : std::optional<Boxf>{};
  if ((beforeRows != 0 && !oldBounds) || (afterRows != 0 && !newBounds)) {
    ShadowChanges_.Unknown();
    return;
  }
  ShadowChanges_.Include(oldBounds, newBounds);
}

bool GeometryRenderer::ChangesShadowSurfaces(std::span<const uint32_t> before,
                                             std::span<const uint32_t> after,
                                             PieceSurface::Source source) const noexcept {
  if (!PiecesVisible_) { return false; }
  return std::ranges::any_of(Pieces_, [&](const Piece &piece) {
    if (!piece.Live || piece.Rows.empty() || piece.Surface.From != source) { return false; }
    const size_t slot = piece.Surface.Index;
    return slot >= before.size() || slot >= after.size() || before[slot] != after[slot];
  });
}

bool GeometryRenderer::TruncatesShadowMaterials(size_t count) const noexcept {
  for (const auto &batch : SubjectBatches_) {
    if (batch.Instances != 0 && batch.MaterialSlot >= count) { return true; }
  }
  for (const auto &piece : Pieces_) {
    if (!PiecesVisible_ || !piece.Live || piece.Rows.empty()) { continue; }
    const auto &slots =
        piece.Surface.From == PieceSurface::Source::Registered ? RegisteredSlotOf_ : SlotOf_;
    if (piece.Surface.Index >= slots.size() || slots[piece.Surface.Index] >= count) { return true; }
  }
  return std::ranges::any_of(Prisms_.Products(), [count](const PrismProduct &product) {
    return !product.Records.empty() && product.Material >= count;
  });
}
}
