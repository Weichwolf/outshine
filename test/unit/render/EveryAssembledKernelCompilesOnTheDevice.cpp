#include <cstdio>
#include <cstdlib>
#include <string>

#include <unistd.h>

#include <SDL3/SDL.h>

#include "Check.h"

#include "CompositeTransmissionStage.h"
#include "KernelShape.h"
#include "MediumMultiScatterStage.h"
#include "MediumRadianceStage.h"
#include "MediumTransmittanceStage.h"
#include "OverlayDraw.h"
#include "PresentStage.h"
#include "SkyStage.h"

using namespace outshine::Render;

namespace {

SDL_GPUDevice *Device = nullptr;

// SDL binds sampler-texture PAIRS: one [[texture(N)]] space holds sampled textures, then
// read-only storage, then read-write storage -- exact by construction. A kernel may sample
// two textures through one sampler, so used sampler slots are AT MOST the declared pairs.
uint32_t Slots(const std::string &text, const char *kind) {
  const std::string needle = std::string("[[") + kind + "(";
  int top = -1;
  for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
    const int slot = std::atoi(text.c_str() + at + needle.size());
    if (slot > top) { top = slot; }
  }
  return (uint32_t)(top + 1);
}

bool Declares(const std::string &source, const ComputeShape &shape) {
  const bool same = Slots(source, "sampler") <= shape.Samplers &&
                    Slots(source, "buffer") == shape.UniformBuffers &&
                    Slots(source, "texture") ==
                        shape.Samplers + shape.ReadOnlyTextures + shape.ReadWriteTextures;
  if (!same) {
    std::printf("NOTE the MSL wants %u samplers, %u buffers, %u textures\n",
                Slots(source, "sampler"), Slots(source, "buffer"), Slots(source, "texture"));
  }
  return same;
}

bool Declares(const std::string &source, const DrawShape &shape) {
  const size_t split = source.find("\nfragment ");
  if (split == std::string::npos) { return false; }
  const std::string vertex = source.substr(0, split);
  const std::string fragment = source.substr(split);
  const bool same = Slots(vertex, "sampler") <= shape.VertexSamplers &&
                    Slots(vertex, "buffer") == shape.VertexUniformBuffers &&
                    Slots(vertex, "texture") == shape.VertexSamplers &&
                    Slots(fragment, "sampler") <= shape.FragmentSamplers &&
                    Slots(fragment, "buffer") == shape.FragmentUniformBuffers &&
                    Slots(fragment, "texture") == shape.FragmentSamplers;
  if (!same) {
    std::printf("NOTE the MSL wants vs %u/%u/%u fs %u/%u/%u (samplers/buffers/textures)\n",
                Slots(vertex, "sampler"), Slots(vertex, "buffer"), Slots(vertex, "texture"),
                Slots(fragment, "sampler"), Slots(fragment, "buffer"),
                Slots(fragment, "texture"));
  }
  return same;
}

bool Compute(const std::string &source, const char *entry, const ComputeShape &shape) {
  if (!Declares(source, shape)) { return false; }
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.num_samplers = shape.Samplers;
  wanted.num_readonly_storage_textures = shape.ReadOnlyTextures;
  wanted.num_readwrite_storage_textures = shape.ReadWriteTextures;
  wanted.num_uniform_buffers = shape.UniformBuffers;
  wanted.threadcount_x = shape.GroupX;
  wanted.threadcount_y = shape.GroupY;
  wanted.threadcount_z = shape.GroupZ;
  SDL_GPUComputePipeline *const made = SDL_CreateGPUComputePipeline(Device, &wanted);
  if (made == nullptr) {
    std::printf("NOTE refused %s: %s\n", entry, SDL_GetError());
    return false;
  }
  SDL_ReleaseGPUComputePipeline(Device, made);
  return true;
}

bool Shader(const std::string &source, const char *entry, SDL_GPUShaderStage stage,
            uint32_t samplers, uint32_t uniforms) {
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = entry;
  wanted.stage = stage;
  wanted.num_samplers = samplers;
  wanted.num_uniform_buffers = uniforms;
  SDL_GPUShader *const made = SDL_CreateGPUShader(Device, &wanted);
  if (made == nullptr) {
    std::printf("NOTE refused %s: %s\n", entry, SDL_GetError());
    return false;
  }
  SDL_ReleaseGPUShader(Device, made);
  return true;
}

bool Both(const std::string &source, const DrawShape &shape) {
  if (!Declares(source, shape)) { return false; }
  return Shader(source, "vs", SDL_GPU_SHADERSTAGE_VERTEX, shape.VertexSamplers,
                shape.VertexUniformBuffers) &&
         Shader(source, "fs", SDL_GPU_SHADERSTAGE_FRAGMENT, shape.FragmentSamplers,
                shape.FragmentUniformBuffers);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO), "SDL video stands for the device");
  Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  CHECK(Device != nullptr, "a headless MSL device stands");
  if (Device == nullptr) { return Report(); }

  CHECK(Compute(MediumTransmittanceStage::KernelSource(), "mediumTransmittanceKernel", MediumTransmittanceStage::KernelShape),
        "**THE TRANSMITTANCE KERNEL THE ENGINE ASSEMBLES COMPILES ON THE DEVICE** -- the text "
        "a stage builds at runtime compiles in the SHAPE the stage declares, and the gate PARSES "
        "the source's own slot annotations back into that shape -- a binding added in the MSL "
        "without its shape field, or a swapped field, refuses here");
  CHECK(Compute(MediumMultiScatterStage::KernelSource(), "mediumMultiScatterKernel", MediumMultiScatterStage::KernelShape),
        "so does the multi-scatter kernel");
  CHECK(Compute(MediumRadianceStage::KernelSource(), "mediumRadianceKernel", MediumRadianceStage::KernelShape),
        "and the radiance kernel");
  CHECK(Both(SkyStage::ShaderSource(), SkyStage::ShaderShape), "the sky's vertex and fragment compile");
  CHECK(Both(PresentStage::ShaderSource(), PresentStage::ShaderShape), "the present blit compiles");
  CHECK(Both(OverlayDraw::ShaderSource(), OverlayDraw::ShaderShape), "the overlay compiles");
  CHECK(Both(CompositeTransmissionStage::ShaderSource(), CompositeTransmissionStage::ShaderShape), "the transmission composite compiles");

  SDL_DestroyGPUDevice(Device);

  char was[4096];
  CHECK(getcwd(was, sizeof was) != nullptr && chdir("/") == 0, "the probe can leave the tree");
  std::string why;
  const std::string lost = SkyStage::ShaderSource(why);
  CHECK(lost.empty() && why.find("src/render/shaders/sky.msl") != std::string::npos,
        "**A SHADER THE TREE CANNOT HAND OVER REFUSES LOUDLY AND NAMES ITS FILE** -- the "
        "sources live as files since 1647, so a process started outside the repository root "
        "gets a refusal that says where the engine looked, never a silent empty kernel");
  CHECK(chdir(was) == 0, "and the probe returns home");

  Covers("IV.8 every kernel and shader the engine assembles at runtime compiles on the device "
         "inside the fast gate -- an unbuildable source refuses in seconds, not in a "
         "five-minute driver run (the 1634 class); tonemap's optioned source and the subject "
         "unit's three follow in the next slice");
  return Report();
}
