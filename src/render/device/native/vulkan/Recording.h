#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_RECORDING_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_RECORDING_H

#ifdef __linux__
#include "Buffer.h"
#include "VulkanImage.h"
#include "Pipeline.h"
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

class Recording final : public Commands {
public:
  explicit Recording(std::shared_ptr<Context> context) : Context_(std::move(context)) {
    ++Context_->Recordings;
  }

  ~Recording() override;

  [[nodiscard]] Result<void> Initialize();
  [[nodiscard]] Result<void> End();
  void CommitImages() noexcept;

  [[nodiscard]] const std::shared_ptr<Context> &Owner() const noexcept { return Context_; }

  [[nodiscard]] VkCommandBuffer Handle() const noexcept { return Commands_; }

  [[nodiscard]] Result<void>
  Copy(const Buffer &source, Buffer &destination, BufferCopy region) override;
  [[nodiscard]] Result<void>
  Copy(const Buffer &source, Texture &destination, TextureTransfer region) override;
  [[nodiscard]] Result<void>
  Copy(const Texture &source, Buffer &destination, TextureTransfer region) override;
  [[nodiscard]] Result<void> Copy(const Texture &source,
                                  TextureRegion sourceRegion,
                                  Texture &destination,
                                  TextureRegion destinationRegion) override;
  [[nodiscard]] Result<void> Dispatch(const ComputePipeline &pipeline,
                                      std::span<const Binding> bindings,
                                      Workgroups groups) override;
  void BeginLabel(std::string_view name) override;
  void EndLabel() override;

private:
  struct BufferAccess {
    VkPipelineStageFlags2 Stage = 0;
    VkAccessFlags2 Access = 0;
  };

  struct ImageAccess {
    VkPipelineStageFlags2 Stage = 0;
    VkAccessFlags2 Access = 0;
    VkImageLayout Layout = VK_IMAGE_LAYOUT_GENERAL;
  };

  struct TrackedImage {
    std::shared_ptr<ImageState> Image;
    std::vector<VkImageLayout> Layouts;
  };

  [[nodiscard]] Result<void> Hold(const std::shared_ptr<BufferState> &buffer);
  [[nodiscard]] Result<void>
  UseImage(const std::shared_ptr<ImageState> &image, TextureView view, ImageAccess access);
  [[nodiscard]] Result<VkDescriptorImageInfo>
  BindImage(const ShaderBinding &declared, const Binding &binding, const PipelineState &pipeline);

  void Use(const BufferState &buffer, ByteRange range, BufferAccess access);
  [[nodiscard]] Result<std::vector<VkDescriptorSet>> AllocateSets(const PipelineState &pipeline);
  [[nodiscard]] Result<std::vector<VkDescriptorSet>>
  BindResources(const VulkanPipeline &pipeline, std::span<const Binding> bindings);
  [[nodiscard]] Result<VkDescriptorBufferInfo> BindBuffer(const ShaderBinding &declared,
                                                          const Binding &binding);

  std::shared_ptr<Context> Context_;
  VkCommandPool Pool_ = VK_NULL_HANDLE;
  VkCommandBuffer Commands_ = VK_NULL_HANDLE;
  VkDescriptorPool Descriptors_ = VK_NULL_HANDLE;
  bool Finished_ = false;
  std::vector<TrackedImage> Images_;
  std::vector<VkImageView> Views_;
  std::vector<std::shared_ptr<SamplerState>> Samplers_;
  std::vector<std::shared_ptr<BufferState>> Buffers_;
  std::vector<std::shared_ptr<PipelineState>> Pipelines_;
};

}
#endif
#endif
