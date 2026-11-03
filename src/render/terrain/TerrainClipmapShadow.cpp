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
      return {};
    }
    if (prepared.error().Kind != Native::ErrorKind::Invalid) { return prepared; }
  }
  if (previousPath != Path_) { ++DrawRevision_; }
  return {};
}

Native::Result<void> TerrainRenderer::CastClipmaps(const PassRecording &into,
                                                   const Mat4f &clipFromWorld,
                                                   const Vec4f &shift) const {
  ClipmapShadowView view;
  const Vec3 &mainShift = Clipmaps_->Shift();
  for (size_t row = 0; row < 4; ++row) {
    for (size_t col = 0; col < 3; ++col) { view.View.At(row, col) = clipFromWorld.At(row, col); }
    double translated = clipFromWorld.At(row, 3);
    for (size_t axis = 0; axis < 3; ++axis) {
      translated += clipFromWorld.At(row, axis) * (shift[axis] - mainShift[axis]);
    }
    view.View.At(row, 3) = static_cast<float>(translated);
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    view.Shift[axis] = static_cast<float>(mainShift[axis]);
  }
  const std::array bindings{Native::Binding{
      .Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}}};
  return Pipelines_->Clipmaps().Encode(into, Clipmaps_->Fields(), bindings, ClipmapPass::Depth);
}
}
