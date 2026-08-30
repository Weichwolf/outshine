#include "SubjectCullStage.h"

#include <cmath>

#include "ShaderFile.h"
#include "ShaderPrelude.h"
#include "SubjectDraw.h"

namespace outshine::Render {
namespace {

std::string Kernel(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/subjectCull.msl", body, error)) { return std::string(); }
  return MslPrelude(error) + body;
}

void PlanesOf(const float mvp[16], float out[24]) {
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
    if (length < 1.0e-12f) {
      for (int c = 0; c < 4; ++c) { out[at * 4 + c] = 0.0f; }
      continue;
    }
    for (int c = 0; c < 4; ++c) { out[at * 4 + c] /= length; }
  }
}

} // namespace

bool SubjectCullStage::Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error) {
  Subjects_ = &subjects;
  if (Pipe_) { return true; }
  const std::string source = KernelSource(error);
  if (source.empty()) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "subjectCullKernel";
  wanted.num_readonly_storage_buffers = KernelShape.ReadOnlyBuffers;
  wanted.num_readwrite_storage_buffers = KernelShape.ReadWriteBuffers;
  wanted.num_uniform_buffers = KernelShape.UniformBuffers;
  wanted.threadcount_x = KernelShape.GroupX;
  wanted.threadcount_y = 1u;
  wanted.threadcount_z = 1u;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(gpu.Device, &wanted);
  if (made == nullptr) {
    error = std::string("the subject cull kernel was refused: ") + SDL_GetError();
    return false;
  }
  Pipe_ = OwnedComputePipeline(gpu.Device, made);
  return true;
}

void SubjectCullStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  Swept_ = 0;
  if (!Pipe_ || Subjects_ == nullptr || into.Dispatch == nullptr) { return; }
  const uint32_t jobs = Subjects_->ClusterJobs();
  if (jobs == 0) { return; }

  struct CullView {
    float Planes[24];
    float Shift[4];
    uint32_t Jobs;
    uint32_t Pad[3];
  } view{};

  PlanesOf(ctx.Mvp16, view.Planes);
  for (int axis = 0; axis < 3; ++axis) {
    view.Shift[axis] = (float)(Subjects_->AnchorM()[axis] + ctx.PreViewTranslation[axis]);
  }
  view.Jobs = jobs;

  const SubjectResidency &resident = Subjects_->Resident();
  SDL_GPUBuffer *const read[4] = {resident.ClusterSpheres.Get(),
                                  resident.ClusterJobs.Get(),
                                  resident.Idx.Get(),
                                  resident.Placed.Get()};
  for (SDL_GPUBuffer *const one : read) {
    if (one == nullptr) { return; }
  }
  SDL_PushGPUComputeUniformData(into.Commands, 0, &view, (uint32_t)sizeof view);
  SDL_BindGPUComputePipeline(into.Dispatch, Pipe_.Get());
  SDL_BindGPUComputeStorageBuffers(into.Dispatch, 0, read, 4);
  SDL_DispatchGPUCompute(into.Dispatch, jobs, 1u, 1u);
  Swept_ = jobs;
}

std::string SubjectCullStage::KernelSource() {
  std::string ignored;
  return Kernel(ignored);
}

std::string SubjectCullStage::KernelSource(std::string &error) {
  return Kernel(error);
}

} // namespace outshine::Render
