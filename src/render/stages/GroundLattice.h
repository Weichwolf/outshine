#ifndef OUTSHINE_RENDER_STAGES_GROUNDLATTICE_H
#define OUTSHINE_RENDER_STAGES_GROUNDLATTICE_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "Gpu.h"
#include "GpuOwned.h"
#include "KernelShape.h"
#include "SubjectTypes.h"

namespace outshine::Render {

class GroundLattice {
public:
  static constexpr int kSide = 34;
  static constexpr uint32_t kNodes = static_cast<uint32_t>(kSide) * static_cast<uint32_t>(kSide);
  static constexpr uint32_t kPages = 512;
  static constexpr uint32_t kGridFloats = 3;
  static constexpr uint32_t kVertices = kNodes + 4u * static_cast<uint32_t>(kSide);
  static constexpr uint32_t kQuads =
      static_cast<uint32_t>(kSide - 1) * static_cast<uint32_t>(kSide - 1);
  static constexpr uint32_t kSkirtQuads = 4u * static_cast<uint32_t>(kSide - 1);
  static constexpr uint32_t kIndices = (kQuads + kSkirtQuads) * 6u;
  static constexpr DrawShape LitShape{.VertexSamplers = 1,
                                      .VertexUniformBuffers = 1,
                                      .FragmentSamplers = kSubjectImages,
                                      .FragmentUniformBuffers = kSubjectFragmentUniforms,
                                      .FragmentStorageBuffers = 3};
  static constexpr DrawShape DepthShape{.VertexSamplers = 1, .VertexUniformBuffers = 1};

  [[nodiscard]] bool Configure(SDL_GPUDevice *device,
                               std::string_view source,
                               std::span<const SDL_GPUColorTargetDescription> targets,
                               std::string &error);
  [[nodiscard]] bool
  ConfigureDepth(SDL_GPUDevice *device, std::string_view depthSource, std::string &error);

  [[nodiscard]] bool SetGrid(std::span<const float> fractions, std::string &error);

  [[nodiscard]] PageId PlacePage(std::span<const float> nodes, std::string &error);
  void ReleasePage(PageId which);
  [[nodiscard]] bool SetInstances(std::span<const GroundInstance> real,
                                  std::span<const GroundInstance> virtual_,
                                  std::string &error);

  void Encode(const PassRecording &into) const;
  void Cast(const PassRecording &into) const;

  [[nodiscard]] uint32_t Instances() const { return RealCount_ + VirtualCount_; }

  [[nodiscard]] uint32_t PagesStanding() const { return PagesLive_; }

  [[nodiscard]] uint32_t Triangles() const { return Instances() * (kIndices / 3u); }

  [[nodiscard]] uint32_t HeldBytes() const {
    return PagesMade_ * kNodes * static_cast<uint32_t>(sizeof(float)) +
           InstanceRoom_ * kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float)) +
           kVertices * kGridFloats * static_cast<uint32_t>(sizeof(float)) +
           kIndices * static_cast<uint32_t>(sizeof(uint32_t));
  }

private:
  [[nodiscard]] bool
  BuildGrid(std::span<const float> fractions, OwnedBuffer &into, std::string &error);
  [[nodiscard]] bool BuildPages(std::string &error);
  void Draw(const PassRecording &into, SDL_GPUGraphicsPipeline *pipeline) const;

  SDL_GPUDevice *Device_ = nullptr;
  OwnedPipeline Lit_;
  OwnedPipeline Depth_;
  OwnedTexture Pages_;
  OwnedSampler Nearest_;
  OwnedBuffer Grid_;
  OwnedBuffer UniformGrid_;
  OwnedBuffer Index_;
  OwnedBuffer Instances_;
  std::vector<PageId> Spare_;
  uint32_t PagesMade_ = 0;
  uint32_t PagesLive_ = 0;
  uint32_t InstanceRoom_ = 0;
  uint32_t RealCount_ = 0;
  uint32_t VirtualCount_ = 0;
};

} // namespace outshine::Render
#endif
