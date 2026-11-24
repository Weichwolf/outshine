#include "TerrainRenderer.h"
#include <array>
#include <cstddef>
#include <expected>
#include <span>

namespace outshine::Render {
namespace {
struct alignas(16) ClipmapShadowView {
  Mat4f View, Previous, Light;
  Vec4f Shift, PreviousShift;
};

static_assert(sizeof(Mat4f) == 64 && sizeof(Vec4f) == 16);
static_assert(sizeof(ClipmapShadowView) == 3 * 64 + 2 * 16 && alignof(ClipmapShadowView) == 16);
static_assert(offsetof(ClipmapShadowView, View) == 0 &&
              offsetof(ClipmapShadowView, Previous) == 64 &&
              offsetof(ClipmapShadowView, Light) == 2 * sizeof(Mat4f) &&
              offsetof(ClipmapShadowView, Shift) == 3 * sizeof(Mat4f) &&
              offsetof(ClipmapShadowView, PreviousShift) == 3 * sizeof(Mat4f) + sizeof(Vec4f));

Mat4f RebaseProjection(Mat4f projection, const Vec3 &offset) {
  for (size_t row = 0; row < 4; ++row) {
    double translated = projection.At(row, 3);
    for (size_t axis = 0; axis < 3; ++axis) {
      translated += projection.At(row, axis) * offset[axis];
    }
    projection.At(row, 3) = static_cast<float>(translated);
  }
  return projection;
}
}

Native::Result<void> TerrainRenderer::PrepareClipmaps(const FrameContext &ctx,
                                                      const Vec3 &anchorM) {
  const Path previousPath = Path_;
  Path_ = Path::Lattice;
  if (Clipmaps_ && Clipmaps_->Complete()) {
    const auto configured = Pipelines_->ConfigureClipmaps(
        *Resident_->Device_, Clipmaps_->Heights().Grid().HalfCells, ClipmapGeometryBytesMost_);
    if (!configured) { return configured; }
    const Vec3 current = anchorM + ctx.PreViewTranslation;
    const Vec3 previous = anchorM + ctx.PrevPreViewTranslation;
    const auto prepared = Clipmaps_->Prepare({.Camera = {{-current[0], current[2]}},
                                              .PreviousCamera = {{-previous[0], previous[2]}},
                                              .Shift = current,
                                              .PreviousShift = previous});
    if (prepared) {
      Path_ = Path::Clipmap;
      const Vec2 &camera = Clipmaps_->LodFocus();
      if (previousPath != Path_ || ClipmapGeneration_ != Clipmaps_->Generation() ||
          ClipmapCamera_ != camera) {
        ++DrawRevision_;
      }
      ClipmapGeneration_ = Clipmaps_->Generation();
      ClipmapCamera_ = camera;
      return Pipelines_->Clipmaps().Select(Clipmaps_->Fields(), ctx.Mvp, ClipmapMain_);
    }
    if (prepared.error().Kind != Native::ErrorKind::Invalid) { return prepared; }
  }
  if (previousPath != Path_) { ++DrawRevision_; }
  return {};
}

Native::Result<void> TerrainRenderer::CastClipmaps(const PassRecording &into,
                                                   ShadowProjections projections,
                                                   const Vec4f &shift) const {
  const Vec3 &mainShift = Clipmaps_->Shift();
  Vec3 offset;
  ClipmapShadowView view{};
  for (size_t axis = 0; axis < 3; ++axis) {
    offset[axis] = shift[axis] - mainShift[axis];
    view.Shift[axis] = static_cast<float>(mainShift[axis]);
  }
  view.View = RebaseProjection(projections.Raster, offset);
  const std::array bindings{Native::Binding{
      .Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}}};
  ClipmapDrawList selection;
  const auto selected = Pipelines_->Clipmaps().Select(
      Clipmaps_->Fields(), RebaseProjection(projections.Patch, offset), selection);
  if (!selected) { return selected; }
  return Pipelines_->Clipmaps().Encode(
      into, Clipmaps_->Fields(), bindings, ClipmapPass::Depth, &selection);
}
}
