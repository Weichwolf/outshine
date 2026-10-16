#include "SurfaceReprojectionStage.h"

#include "StagePipelines.h"
#include <span>
#include "TransformMatrix.h"
#include "math/Vec4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace outshine::Render {
namespace {
constexpr float kHalfTurnDeg = 180;

struct alignas(16) ReprojectionUniform {
  Mat4f SourceFromTarget;
  Mat4f TargetFromSource;
  Vec4f TargetExtent;
};

static_assert(sizeof(ReprojectionUniform) == 2 * sizeof(Mat4f) + sizeof(Vec4f));
static_assert(alignof(ReprojectionUniform) == 16);
static_assert(offsetof(ReprojectionUniform, SourceFromTarget) == 0);
static_assert(offsetof(ReprojectionUniform, TargetFromSource) == 64);
static_assert(offsetof(ReprojectionUniform, TargetExtent) == 128);

bool Perspective(const Lens &lens) {
  return lens.OrthoM == 0 && std::isfinite(lens.FovDeg) && lens.FovDeg > 0 &&
         lens.FovDeg < kHalfTurnDeg && std::isfinite(lens.NearM) && lens.NearM > 0 &&
         (lens.FarM == 0 || lens.FarM > lens.NearM) && std::isfinite(lens.WidePx) &&
         lens.WidePx > 0 && std::isfinite(lens.HighPx) && lens.HighPx > 0 &&
         std::ranges::all_of(lens.Jitter, [](float x) { return std::isfinite(x); });
}

std::expected<ReprojectionUniform, std::string> Reprojection(const CameraBasis &sourceCamera,
                                                             const Lens &sourceLens,
                                                             const CameraBasis &targetCamera,
                                                             const Lens &targetLens) {
  if (sourceCamera.EyeM != targetCamera.EyeM ||
      !std::ranges::all_of(sourceCamera.EyeM, [](double x) { return std::isfinite(x); })) {
    return std::unexpected(
        "surface reprojection requires the captured camera position; retain native geometry");
  }
  if (!Perspective(sourceLens) || !Perspective(targetLens)) {
    return std::unexpected("surface reprojection requires finite perspective projections");
  }
  const Mat4f source = sourceLens.ViewProjection(sourceCamera);
  const Mat4f target = targetLens.ViewProjection(targetCamera);
  Mat4 sourceMatrix;
  Mat4 targetMatrix;
  for (size_t at = 0; at < source.Elements().size(); ++at) {
    sourceMatrix[at] = source[at];
    targetMatrix[at] = target[at];
  }
  Mat4 sourceInverse;
  Mat4 targetInverse;
  if (!InverseMatrix(sourceMatrix, sourceInverse) || !InverseMatrix(targetMatrix, targetInverse)) {
    return std::unexpected("surface reprojection requires invertible view projections");
  }
  const Mat4 sourceFromTarget = sourceMatrix * targetInverse;
  const Mat4 targetFromSource = targetMatrix * sourceInverse;
  ReprojectionUniform result{};
  for (size_t at = 0; at < source.Elements().size(); ++at) {
    result.SourceFromTarget[at] = static_cast<float>(sourceFromTarget[at]);
    result.TargetFromSource[at] = static_cast<float>(targetFromSource[at]);
  }
  result.TargetExtent = {
      {static_cast<float>(targetLens.WidePx), static_cast<float>(targetLens.HighPx), 0, 0}};
  const auto finite = [](float x) { return std::isfinite(x); };
  if (!std::ranges::all_of(result.SourceFromTarget, finite) ||
      !std::ranges::all_of(result.TargetFromSource, finite) ||
      !std::ranges::all_of(result.TargetExtent, finite)) {
    return std::unexpected("surface reprojection exceeds finite GPU coordinates");
  }
  return result;
}

}

Native::Result<void>
SurfaceReprojectionStage::Configure(const Gpu &gpu, Capture source, Native::Sampler *sampler) {
  constexpr std::array<Resource, 5> outputs{Resource::SceneHdr,
                                            Resource::SceneShadingNormal,
                                            Resource::SceneSurfaceIdentity,
                                            Resource::SceneSurfaceBase,
                                            Resource::SceneSurfaceMetalRough};
  if (gpu.Device == nullptr || gpu.SceneColours.Size() != outputs.size() ||
      !std::equal(outputs.begin(), outputs.end(), gpu.SceneColours.begin()) || sampler == nullptr ||
      !std::ranges::all_of(source.DepthNormalIdentityBaseMetalRough,
                           [](auto *texture) { return texture != nullptr; })) {
    return std::unexpected(Native::Error{
        .Kind = Native::ErrorKind::Invalid,
        .Message =
            "surface reprojection requires complete captured channels and matching targets"});
  }
  auto checked = Reprojection(source.Camera, source.Projection, source.Camera, source.Projection);
  if (!checked) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(checked.error())});
  }
  std::array<Native::ColourTarget, 5> targets{};
  const std::array formats{gpu.HdrFormat,
                           Native::Format::Rgba16Float,
                           Native::Format::Rgba32Float,
                           Native::Format::Rgba16Float,
                           Native::Format::Rg16Float};
  for (size_t at = 0; at < targets.size(); ++at) { targets[at].Texels = formats[at]; }
  targets[0].Write = {};
  auto made =
      CreateFullscreenPipeline(*gpu.Device,
                               "build/shaders/surfaceReprojection.frag.spv",
                               targets,
                               Native::DepthTarget{.Compare = Native::DepthCompare::GreaterEqual});
  if (!made) { return std::unexpected(made.error()); }
  Pipe_ = std::move(*made);
  Source_ = source;
  Sampler_ = sampler;
  return {};
}

Native::Result<void> SurfaceReprojectionStage::Encode(const CameraBasis &camera,
                                                      const Lens &projection,
                                                      const PassRecording &into) const {
  if (!Pipe_ || into.Commands == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "surface reprojection requires a configured native raster pass"});
  }
  auto uniforms = Reprojection(Source_.Camera, Source_.Projection, camera, projection);
  if (!uniforms) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = std::move(uniforms.error())});
  }
  std::array<Native::Binding, ShaderShape.FragmentSamplers + 1> bindings;
  for (uint32_t at = 0; at < ShaderShape.FragmentSamplers; ++at) {
    bindings[at] = {.Set = 2,
                    .Slot = at,
                    .Value = Native::SampledTexture{
                        .Image = {.Image = Source_.DepthNormalIdentityBaseMetalRough[at]},
                        .Sampling = Sampler_}};
  }
  bindings.back() = {.Set = 3,
                     .Slot = 0,
                     .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&*uniforms, 1))}};
  return into.Commands->Draw({.State = {.Pipeline = Pipe_.get(),
                                        .Bindings = bindings,
                                        .Vertices = {},
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 3}});
}
}
