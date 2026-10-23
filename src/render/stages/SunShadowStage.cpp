#include "TerrainRenderer.h"
#include "BatchVisibility.h"
#include "math/Units.h"
#include "math/Mat4.h"
#include "SunShadowStage.h"
#include "math/Vec3.h"
#include "math/Box.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <expected>
#include <span>
#include <utility>
#include "GeometryRenderer.h"

namespace outshine::Render {

constexpr size_t kShadowViewBytes = 80;
constexpr double kParallelLightCrossSquared = 1.0e-12;
constexpr std::array<double, 3> kCameraShadowRadiiM = {{256.0, 1024.0, 4096.0}};
constexpr float kDepthRoundingAllowance = 3.0f * std::numeric_limits<float>::epsilon();

namespace {
struct alignas(16) ShadowView {
  Mat4f Light{};
  Vec4f Shift{};
};

static_assert(sizeof(ShadowView) == kShadowViewBytes && alignof(ShadowView) == 16);
static_assert(offsetof(ShadowView, Light) == 0 && offsetof(ShadowView, Shift) == sizeof(Mat4f));

std::array<size_t, 2> ShadowTile(size_t region) {
  return {{region % 2, region / 2}};
}
}

Native::Result<void> SunShadowStage::Configure(GeometryRenderer &subjects, const Gpu &gpu) {
  if (gpu.Device == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "shadow pipelines require a device"});
  }
  if (Device_ != gpu.Device || !DepthOnly_ || !ClearTile_) {
    auto depth = ConfigureDepthOnly(*gpu.Device);
    if (!depth) { return std::unexpected(depth.error()); }
    auto masked = ConfigureMasked(*gpu.Device);
    if (!masked) { return std::unexpected(masked.error()); }
    auto clear = ConfigureTileClear(*gpu.Device);
    if (!clear) { return std::unexpected(clear.error()); }
    DepthOnly_ = std::move(*depth);
    ClearTile_ = std::move(*clear);
    Masked_ = std::move(*masked);
    Device_ = gpu.Device;
  }
  Cache_.Invalidate();
  Binds(subjects);
  return {};
}

void SunShadowStage::Declare(Overhead sky, double radiusM, bool cameraCentred) {
  const Vec3 toSun = {{sky.ToSun[0], sky.ToSun[1], sky.ToSun[2]}};
  const Vec3 up = {{sky.Up[0], sky.Up[1], sky.Up[2]}};
  if (ToSun_ != toSun || Up_ != up || RadiusM_ != radiusM || CameraCentred_ != cameraCentred) {
    Cache_.Invalidate();
  }
  CameraCentred_ = cameraCentred;
  for (int axis = 0; axis < 3; ++axis) {
    ToSun_[axis] = static_cast<double>(sky.ToSun[axis]);
    Up_[axis] = static_cast<double>(sky.Up[axis]);
  }
  RadiusM_ = radiusM;
  double sunLength = 0.0;
  double upLength = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    sunLength += ToSun_[axis] * ToSun_[axis];
    upLength += Up_[axis] * Up_[axis];
  }
  Declared_ = std::isfinite(radiusM) && radiusM > 0.0 && std::isfinite(sunLength) &&
              sunLength > 0.0 && std::isfinite(upLength) && upLength > 0.0;
}

Vec3 SunShadowStage::CasterCentre() const {
  const Vec3 anchor = Subjects_ != nullptr ? Subjects_->AnchorM() : Vec3{};
  Box bounds;
  if (Subjects_ != nullptr) {
    const std::vector<double> &placed = Subjects_->Placements();
    const size_t slots = placed.size() / 16u;
    for (size_t slot = 0; slot < slots && slot < CastsBelow_; ++slot) {
      const double *const model = placed.data() + slot * 16u;
      bounds.Cover(Vec3{{model[12], model[13], model[14]}});
    }
    const auto prisms = Subjects_->Prisms().Bounds();
    if (!prisms.Empty()) {
      bounds.Cover(Vec3{{static_cast<double>(prisms.Min[0]),
                         static_cast<double>(prisms.Min[1]),
                         static_cast<double>(prisms.Min[2])}});
      bounds.Cover(Vec3{{static_cast<double>(prisms.Max[0]),
                         static_cast<double>(prisms.Max[1]),
                         static_cast<double>(prisms.Max[2])}});
    }
  }
  return bounds.Empty() ? anchor : bounds.Middle() + anchor;
}

void SunShadowStage::Build(const Vec3 &preView) {
  Vec3 forward = {{-ToSun_[0], -ToSun_[1], -ToSun_[2]}};
  double length = 0.0;
  for (const double axis : forward) { length += axis * axis; }
  length = std::sqrt(length);
  for (double &axis : forward) { axis /= length; }

  Vec3 right = {{Up_[1] * forward[2] - Up_[2] * forward[1],
                 Up_[2] * forward[0] - Up_[0] * forward[2],
                 Up_[0] * forward[1] - Up_[1] * forward[0]}};
  if (right[0] * right[0] + right[1] * right[1] + right[2] * right[2] <
      kParallelLightCrossSquared) {
    const Vec3 pole = std::abs(forward[0]) < 0.9 ? Vec3{{1, 0, 0}} : Vec3{{0, 1, 0}};
    right = {{pole[1] * forward[2] - pole[2] * forward[1],
              pole[2] * forward[0] - pole[0] * forward[2],
              pole[0] * forward[1] - pole[1] * forward[0]}};
  }
  const double rLength = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
  for (double &axis : right) { axis /= rLength; }
  const Vec3 upward = {{forward[1] * right[2] - forward[2] * right[1],
                        forward[2] * right[0] - forward[0] * right[2],
                        forward[0] * right[1] - forward[1] * right[0]}};

  const Vec3 centre =
      CameraCentred_ ? Vec3{{-preView[0], -preView[1], -preView[2]}} : CasterCentre();
  double radiusM = RadiusM_;
  if (CameraCentred_) {
    const Vec3 delta = centre - CasterCentre();
    const double quantumM = kCameraShadowRadiiM.front();
    radiusM +=
        std::ceil(std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]) /
                  quantumM) *
        quantumM;
    radiusM = std::ceil(radiusM / quantumM) * quantumM * (1.0 + 2.0 / kShadowTilePx);
  }
  ShadowCentreM_ = centre;
  BuildRegions({.Right = right, .Upward = upward, .Forward = forward}, centre, radiusM, preView);
}

void SunShadowStage::BuildRegions(const LightBasis &basis,
                                  const Vec3 &centre,
                                  double radiusM,
                                  const Vec3 &preView) {
  Vec3 centreLight{};
  Vec3 casterLight{};
  const Vec3 casterCentre = CasterCentre();
  for (size_t axis = 0; axis < 3; ++axis) {
    centreLight[0] += basis.Right[axis] * centre[axis];
    centreLight[1] += basis.Upward[axis] * centre[axis];
    centreLight[2] += basis.Forward[axis] * centre[axis];
    casterLight[2] += basis.Forward[axis] * casterCentre[axis];
  }
  const double depthSpanM = 4.0 * RadiusM_;
  const double farAlong = casterLight[2] + 2.0 * RadiusM_;
  const std::array<double, kSunShadowRegions> radii{std::min(kCameraShadowRadiiM[0], radiusM),
                                                    std::min(kCameraShadowRadiiM[1], radiusM),
                                                    std::min(kCameraShadowRadiiM[2], radiusM),
                                                    radiusM};
  const size_t base = CameraCentred_ ? kSunShadowRegions - 1 : 0;
  std::array<Vec3, kSunShadowRegions> snapped{};
  for (size_t region = 0; region < RegionCount(); ++region) {
    const double extentM = CameraCentred_ ? radii[region] : radiusM;
    const double texelM = 2.0 * extentM / kShadowTilePx;
    for (size_t axis = 0; axis < 2; ++axis) {
      snapped[region][axis] = std::floor(centreLight[axis] / texelM) * texelM;
    }
    Mat4 &projection = Projections_[region];
    projection = {};
    for (size_t axis = 0; axis < 3; ++axis) {
      projection[axis * 4] = basis.Right[axis] / extentM;
      projection[axis * 4 + 1] = basis.Upward[axis] / extentM;
      projection[axis * 4 + 2] = -basis.Forward[axis] / depthSpanM;
    }
    projection[12] = -snapped[region][0] / extentM;
    projection[13] = -snapped[region][1] / extentM;
    projection[14] = farAlong / depthSpanM;
    projection[15] = 1.0;
    Static_[region] = projection;
    for (size_t row = 0; row < 3; ++row) {
      for (size_t axis = 0; axis < 3; ++axis) {
        projection[12 + row] -= projection[axis * 4 + row] * preView[axis];
      }
    }
    const float bias =
        CameraCentred_ ? std::max(kDepthRoundingAllowance,
                                  static_cast<float>(std::max(0.02, 0.25 * texelM) / depthSpanM))
                       : 1.0f / kShadowTilePx;
    const auto tile = ShadowTile(region);
    RegionUniforms_[region].Depth =
        CameraCentred_ ? Vec4f{{static_cast<float>(1.0 / depthSpanM),
                                static_cast<float>((farAlong - centreLight[2]) / depthSpanM),
                                0,
                                0}}
                       : Vec4f{{1, 0, 0, 0}};
    RegionUniforms_[region].Atlas = {
        {static_cast<float>(tile[0]) * 0.5f, static_cast<float>(tile[1]) * 0.5f, 0.5f, bias}};
  }
  LightFromWorld_ = Projections_[base];
  if (CameraCentred_) {
    for (size_t axis = 0; axis < 3; ++axis) {
      LightFromWorld_[axis * 4 + 2] = -basis.Forward[axis];
    }
    LightFromWorld_[14] = 0;
  }
  const double baseRadiusM = CameraCentred_ ? radii[base] : radiusM;
  for (size_t region = 0; region < RegionCount(); ++region) {
    const double extentM = CameraCentred_ ? radii[region] : radiusM;
    const auto scale = static_cast<float>(baseRadiusM / extentM);
    RegionUniforms_[region].Transform = {
        {scale,
         scale,
         static_cast<float>((snapped[base][0] - snapped[region][0]) / extentM),
         static_cast<float>((snapped[base][1] - snapped[region][1]) / extentM)}};
  }
}

void SunShadowStage::Prepare(const FrameContext &ctx) {
  Casting_ = false;
  PreservesAtlas_ = false;
  CastBatches_ = 0;
  if (!Declared_ || Subjects_ == nullptr) { return; }
  if (Cache_.Submitted()) {
    for (size_t region = 0; region < RegionCount(); ++region) {
      AcceptedRaster_[region] = RasterUpdates_[region].Next;
    }
  }
  Build(ctx.PreViewTranslation);
  const uint64_t stands = Subjects_->Generation();
  const uint64_t ground = Subjects_->Ground().Generation();
  const bool reusable =
      Cache_.Submitted() && stands == PreparedGeneration_ && ground == PreparedGroundGeneration_;
  size_t changed = 0;
  for (size_t region = 0; region < RegionCount(); ++region) {
    RasterUpdates_[region] = PrepareShadowRaster(
        AcceptedRaster_[region], Static_[region], Subjects_->AnchorM(), kShadowTilePx, !reusable);
    for (size_t axis = 0; axis < 2; ++axis) {
      RegionUniforms_[region].Depth[2 + axis] =
          static_cast<float>(RasterUpdates_[region].Origin[axis]);
    }
    DirtyRegions_[region] = RasterUpdates_[region].Count > 0;
    changed += DirtyRegions_[region] ? 1u : 0u;
  }
  if (changed == 0) { return; }
  PreservesAtlas_ = reusable;
  Cache_.Invalidate();
  Casting_ = true;
  PreparedGeneration_ = stands;
  PreparedGroundGeneration_ = ground;
}

Native::Result<void> SunShadowStage::Encode([[maybe_unused]] const FrameContext &ctx,
                                            const PassRecording &into) {
  if (!Casting_ || !Cache_.NeedsRecording()) { return {}; }
  if (!DepthOnly_ || Subjects_ == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "shadow recording requires native resources"});
  }
  CastBatches_ = 0;
  for (size_t region = 0; region < RegionCount(); ++region) {
    if (!DirtyRegions_[region]) { continue; }
    const auto accepted = Cast(region, into);
    if (!accepted) { return accepted; }
  }
  into.Submission.Record(Stage::LightVisibility, Cache_);
  return {};
}

Native::Result<void> SunShadowStage::Cast(size_t region, const PassRecording &into) {
  const auto tile = ShadowTile(region);
  const Native::Viewport square{.X = static_cast<float>(tile[0]) * kShadowTilePx,
                                .Y = static_cast<float>(tile[1]) * kShadowTilePx,
                                .Width = kShadowTilePx,
                                .Height = kShadowTilePx};
  const auto &update = RasterUpdates_[region];
  for (size_t at = 0; at < update.Count; ++at) {
    const auto &patch = update.Patches[at];
    const Native::Scissor clip{.X = static_cast<uint32_t>(tile[0]) * kShadowTilePx + patch.X,
                               .Y = static_cast<uint32_t>(tile[1]) * kShadowTilePx + patch.Y,
                               .Width = patch.Width,
                               .Height = patch.Height};
    const PassRecording recording{
        .Commands = into.Commands, .Submission = into.Submission, .Area = square, .Clip = clip};
    if (PreservesAtlas_) {
      const auto cleared = into.Commands->Draw({.State = {.Pipeline = ClearTile_.get(),
                                                          .Bindings = {},
                                                          .Vertices = {},
                                                          .Area = square,
                                                          .Clip = clip},
                                                .Range = {.Count = 3}});
      if (!cleared) { return cleared; }
    }
    ShadowView uniform{.Light = ShadowPatchProjection(update.Next, patch)};
    const auto selection = ShadowPatchCullProjection(uniform.Light, patch, kShadowTilePx);
    const auto bytes = std::as_bytes(std::span(&uniform, 1));
    const std::array bindings{
        Native::Binding{.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = bytes}}};
    const auto ground = Subjects_->Ground().Cast(recording, selection, uniform.Shift, bindings);
    if (!ground) { return ground; }
    const auto subjects = CastSubjects(recording, bytes, selection);
    if (!subjects) { return subjects; }
    const auto prisms = Subjects_->EncodePrismShadow(
        recording, {.ClipFromWorld = uniform.Light, .Shift = uniform.Shift.Axis});
    if (!prisms) { return prisms; }
  }
  return {};
}

Native::Result<void> SunShadowStage::CastSubjects(const PassRecording &into,
                                                  std::span<const std::byte> view,
                                                  const Mat4f &selection) {
  const auto &resident = Subjects_->Resident();
  const auto &batches = Subjects_->Drawn();
  if (batches.empty()) { return {}; }
  if (!resident.Buffer(GeometryResidency::Stream::Vertex) ||
      !resident.Buffer(GeometryResidency::Stream::Index)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "shadow subjects have no native vertex/index data"});
  }
  FrameContext frame;
  std::ranges::copy(selection, frame.Mvp.begin());
  const BatchFrustum visible(frame, {});
  const auto subjectRows = Subjects_->SubjectRows();
  for (size_t batchIndex = 0; batchIndex < batches.size(); ++batchIndex) {
    const auto &batch = batches[batchIndex];
    if (batch.ModelSlot >= CastsBelow_ && batch.ModelSlot < subjectRows) { continue; }
    const auto instances = batch.ModelSlot < subjectRows
                               ? std::min(batch.Instances, CastsBelow_ - batch.ModelSlot)
                               : batch.Instances;
    if (instances == 0 || visible.Outside(batch.WorldSphere)) { continue; }
    const auto *const pipeline =
        batch.Kind == SurfaceKind::Masked ? MaskedPipeline(batch.Layout) : DepthOnly_.get();
    if (pipeline == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "shadow pipeline is missing"});
    }
    const auto cast =
        Subjects_->EncodeShadow(into, *pipeline, batchIndex, visible, instances, view);
    if (!cast) { return std::unexpected(cast.error()); }
    CastBatches_ += *cast;
  }
  return {};
}
}
