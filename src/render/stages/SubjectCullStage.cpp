#include "Units.h"
#include "math/Mat4.h"
#include "math/Vec4.h"
#include "SubjectCullStage.h"

#include <span>
#include <array>
#include <cstdint>
#include <string>

#include <atomic>
#include <cmath>
#include <cstdio>

#include "ShaderFile.h"
#include "ShaderPrelude.h"
#include "SubjectDraw.h"

namespace outshine::Render {

namespace {

struct CullView {
  std::array<float, 24> Planes{};
  Vec4f Shift;
  uint32_t Jobs;
  float ErrorPerMetre;
  std::array<uint32_t, 2> Pad{};

  Mat4f Clip{};
  std::array<uint32_t, 4> PyramidWide{};
  std::array<uint32_t, 4> PyramidHigh{};
  std::array<uint32_t, 4> PyramidAt{};
  uint32_t Occludes;
  std::array<uint32_t, 3> Pad2{};
};

static_assert(sizeof(CullView) % 16u == 0u, "the cull uniform keeps its float4x4 aligned");

std::string Kernel(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/subjectCull.msl", body, error)) { return {}; }
  return MslPrelude(error) + body;
}

std::atomic<float> gErrorPerMetre{0.0f};
std::atomic<uint32_t> gJobsSwept{0};

void PlanesOf(const Mat4f &mvp, std::span<float, 24> out) {
  const auto row = [mvp](int r, int c) { return mvp[c * 4 + r]; };
  for (int at = 0; at < 6; ++at) {
    const int axis = at / 2;
    const bool minus = at % 2 == 1;
    for (int c = 0; c < 4; ++c) {
      const float w = row(3, c);
      const float a = row(axis, c);
      out[at * 4 + c] = axis == 2 ? (minus ? w - a : a) : (minus ? w - a : w + a);
    }
    const float length = std::sqrt(out[at * 4] * out[at * 4] + out[at * 4 + 1] * out[at * 4 + 1] +
                                   out[at * 4 + 2] * out[at * 4 + 2]);
    if (length < static_cast<float>(kParallelCross)) {
      for (int c = 0; c < 4; ++c) { out[at * 4 + c] = 0.0f; }
      continue;
    }
    for (int c = 0; c < 4; ++c) { out[at * 4 + c] /= length; }
  }
}

} // namespace

bool SubjectCullStage::Pipeline(const Gpu &gpu,
                                const char *entry,
                                const ComputeShape &shape,
                                OwnedComputePipeline &into,
                                std::string &error) {
  if (into) { return true; }
  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.num_readonly_storage_buffers = shape.ReadOnlyBuffers;
  wanted.num_readwrite_storage_buffers = shape.ReadWriteBuffers;
  wanted.num_uniform_buffers = shape.UniformBuffers;
  wanted.threadcount_x = shape.GroupX;
  wanted.threadcount_y = 1u;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the subject cull's ") + entry + " was refused: " + SDL_GetError();
    return false;
  }
  into = OwnedComputePipeline(gpu.Device, made);
  return true;
}

bool SubjectCullStage::Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error) {
  Subjects_ = &subjects;
  return Pipeline(gpu, "subjectCullKernel", CullShape, Cull_, error) &&
         Pipeline(gpu, "subjectScanKernel", ScanShape, Scan_, error) &&
         Pipeline(gpu, "subjectCompactKernel", CompactShape, Compact_, error);
}

uint32_t SubjectCullStage::JobsSweptTaken() {
  return gJobsSwept.load(std::memory_order_relaxed);
}

float SubjectCullStage::ErrorPerMetreTaken() {
  return gErrorPerMetre.load(std::memory_order_relaxed);
}

uint32_t SubjectCullStage::Standing(const FrameContext &ctx, void *view) {
  if (Subjects_ == nullptr) { return 0; }
  const uint32_t jobs = Subjects_->ClusterJobs();
  if (jobs == 0) { return 0; }
  auto &into = *static_cast<CullView *>(view);
  into = CullView{};
  PlanesOf(ctx.Mvp, into.Planes);
  for (int axis = 0; axis < 3; ++axis) {
    into.Shift[axis] =
        static_cast<float>(Subjects_->AnchorM()[axis] + ctx.PreViewTranslation[axis]);
  }
  into.Jobs = jobs;
  for (int at = 0; at < 16; ++at) { into.Clip[at] = ctx.Mvp[at]; }
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    into.PyramidWide[level] = Pyramid_.Wide[level];
    into.PyramidHigh[level] = Pyramid_.High[level];
    into.PyramidAt[level] = Pyramid_.At[level];
  }
  into.Occludes = PyramidBuffer_ != nullptr && Stood_ ? 1u : 0u;
  const float *const up = into.Planes.data() + static_cast<size_t>(2U * 4U);
  const float *const down = into.Planes.data() + static_cast<size_t>(3U * 4U);
  const float between = up[0] * down[0] + up[1] * down[1] + up[2] * down[2];
  const float yfov = std::acos(std::fmin(std::fmax(-between, -1.0f), 1.0f));
  const float halfTangent = std::tan(0.5f * yfov);
  into.ErrorPerMetre = halfTangent > static_cast<float>(kLeastRunM) && HeightPx_ > 0.0f
                           ? HeightPx_ * 0.5f / halfTangent
                           : 0.0f;
  gErrorPerMetre.store(into.ErrorPerMetre, std::memory_order_relaxed);
  return jobs;
}

void SubjectCullStage::EncodeCull(const FrameContext &ctx, const PassRecording &into) {
  Swept_ = 0;
  CullView view{};
  const uint32_t jobs = Standing(ctx, &view);
  if (jobs == 0 || !Cull_ || into.Dispatch == nullptr) { return; }
  const SubjectResidency &resident = Subjects_->Resident();
  std::array<SDL_GPUBuffer *const, 5> read = {
      resident.ClusterSpheres.Get(),
      resident.ClusterJobs.Get(),
      resident.Placed.Get(),
      resident.DrawArgs.Get(),
      PyramidBuffer_ != nullptr ? PyramidBuffer_ : resident.ClusterSpheres.Get()};
  for (const SDL_GPUBuffer *const one : read) {
    if (one == nullptr) { return; }
  }
  SDL_PushGPUComputeUniformData(into.Commands, 0, &view, static_cast<uint32_t>(sizeof view));
  SDL_BindGPUComputePipeline(into.Dispatch, Cull_.Get());
  SDL_BindGPUComputeStorageBuffers(into.Dispatch, 0, read.data(), 5);
  Stood_ = true;
  SDL_DispatchGPUCompute(into.Dispatch, (jobs + CullShape.GroupX - 1u) / CullShape.GroupX, 1u, 1u);
  Swept_ = jobs;
  gJobsSwept.store(jobs, std::memory_order_relaxed);
}

void SubjectCullStage::EncodeScan(const FrameContext &ctx, const PassRecording &into) {
  CullView view{};
  const uint32_t jobs = Standing(ctx, &view);
  const uint32_t batches = Subjects_ != nullptr ? Subjects_->ClusterBatchRows() : 0u;
  if (jobs == 0 || batches == 0 || !Scan_ || into.Dispatch == nullptr) { return; }
  const SubjectResidency &resident = Subjects_->Resident();
  std::array<SDL_GPUBuffer *const, 2> read = {resident.ClusterKept.Get(),
                                              resident.ClusterBatches.Get()};
  for (const SDL_GPUBuffer *const one : read) {
    if (one == nullptr) { return; }
  }
  SDL_PushGPUComputeUniformData(into.Commands, 0, &view, static_cast<uint32_t>(sizeof view));
  SDL_BindGPUComputePipeline(into.Dispatch, Scan_.Get());
  SDL_BindGPUComputeStorageBuffers(into.Dispatch, 0, read.data(), 2);
  SDL_DispatchGPUCompute(into.Dispatch, batches, 1u, 1u);
}

void SubjectCullStage::EncodeCompact(const FrameContext &ctx, const PassRecording &into) {
  CullView view{};
  const uint32_t jobs = Standing(ctx, &view);
  if (jobs == 0 || !Compact_ || into.Dispatch == nullptr) { return; }
  const SubjectResidency &resident = Subjects_->Resident();
  std::array<SDL_GPUBuffer *const, 4> read = {resident.ClusterJobs.Get(),
                                              resident.Idx.Get(),
                                              resident.ClusterSlot.Get(),
                                              resident.DrawArgs.Get()};
  for (const SDL_GPUBuffer *const one : read) {
    if (one == nullptr) { return; }
  }
  SDL_PushGPUComputeUniformData(into.Commands, 0, &view, static_cast<uint32_t>(sizeof view));
  SDL_BindGPUComputePipeline(into.Dispatch, Compact_.Get());
  SDL_BindGPUComputeStorageBuffers(into.Dispatch, 0, read.data(), 4);
  SDL_DispatchGPUCompute(into.Dispatch, jobs, 1u, 1u);
}

std::string SubjectCullStage::KernelSource() {
  std::string ignored;
  return Kernel(ignored);
}

std::string SubjectCullStage::KernelSource(std::string &error) {
  return Kernel(error);
}

} // namespace outshine::Render
