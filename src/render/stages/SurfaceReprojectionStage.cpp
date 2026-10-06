#include "SurfaceReprojectionStage.h"

#include "ShaderFile.h"
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

std::expected<void, std::string>
SurfaceReprojectionStage::Configure(const Gpu &gpu, Capture source, SDL_GPUSampler *sampler) {
  constexpr std::array<Resource, 5> outputs{Resource::SceneHdr,
                                            Resource::SceneShadingNormal,
                                            Resource::SceneSurfaceIdentity,
                                            Resource::SceneSurfaceBase,
                                            Resource::SceneSurfaceMetalRough};
  if (gpu.Device == nullptr || gpu.SceneColours.Size() != outputs.size() ||
      !std::equal(outputs.begin(), outputs.end(), gpu.SceneColours.begin()) || sampler == nullptr ||
      !std::ranges::all_of(source.DepthNormalIdentityBaseMetalRough,
                           [](auto *texture) { return texture != nullptr; })) {
    return std::unexpected(
        "surface reprojection requires complete captured channels and matching targets");
  }
  auto checked = Reprojection(source.Camera, source.Projection, source.Camera, source.Projection);
  if (!checked) { return std::unexpected(std::move(checked.error())); }
  std::string error;
  const OwnedShader vertex(gpu.Device,
                           ShaderFrom(gpu.Device,
                                      "build/shaders/fullscreen.vert.spv",
                                      SDL_GPU_SHADERSTAGE_VERTEX,
                                      ShaderShape,
                                      error));
  const OwnedShader fragment(gpu.Device,
                             ShaderFrom(gpu.Device,
                                        "build/shaders/surfaceReprojection.frag.spv",
                                        SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        ShaderShape,
                                        error));
  if (!vertex || !fragment) { return std::unexpected(std::move(error)); }
  std::array<SDL_GPUColorTargetDescription, 5> targets{};
  constexpr std::array formats{SDL_GPU_TEXTUREFORMAT_INVALID,
                               SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                               SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT};
  for (size_t at = 0; at < targets.size(); ++at) {
    targets[at].format = at == 0 ? gpu.HdrFormat : formats[at];
  }
  targets[0].blend_state.enable_color_write_mask = true;
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.depth_stencil_state.enable_depth_test = true;
  pipeline.depth_stencil_state.enable_depth_write = true;
  pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
  pipeline.target_info.color_target_descriptions = targets.data();
  pipeline.target_info.num_color_targets = static_cast<uint32_t>(targets.size());
  pipeline.target_info.has_depth_stencil_target = true;
  pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  auto *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (made == nullptr) { return std::unexpected(SDL_GetError()); }
  Pipe_ = OwnedPipeline(gpu.Device, made);
  Source_ = source;
  Sampler_ = sampler;
  return {};
}

std::expected<void, std::string> SurfaceReprojectionStage::Encode(const CameraBasis &camera,
                                                                  const Lens &projection,
                                                                  const PassRecording &into) const {
  if (!Pipe_ || into.Commands == nullptr || into.Pass == nullptr) {
    return std::unexpected("surface reprojection requires a configured raster pass");
  }
  auto uniforms = Reprojection(Source_.Camera, Source_.Projection, camera, projection);
  if (!uniforms) { return std::unexpected(std::move(uniforms.error())); }
  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe_.Get());
  SDL_PushGPUFragmentUniformData(into.Commands, 0, &*uniforms, sizeof(*uniforms));
  std::array<SDL_GPUTextureSamplerBinding, 5> feeds{};
  for (size_t at = 0; at < feeds.size(); ++at) {
    feeds[at] = {.texture = Source_.DepthNormalIdentityBaseMetalRough[at], .sampler = Sampler_};
  }
  SDL_BindGPUFragmentSamplers(into.Pass, 0, feeds.data(), static_cast<uint32_t>(feeds.size()));
  SDL_DrawGPUPrimitives(into.Pass, 3, 1, 0, 0);
  return {};
}

}
