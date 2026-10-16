#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_RECORDING_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_RECORDING_H

#ifdef __linux__
#include "Buffer.h"
#include "BindingStorage.h"
#include "RenderState.h"
#include "Presenter.h"
#include "VulkanImage.h"
#include "Pipeline.h"
#include "Timing.h"
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

class Recording final : public Commands {
public:
  explicit Recording(std::shared_ptr<Context> context)
      : Context_(std::move(context)), Timings_(*Context_), Storage_(Context_) {
    ++Context_->Recordings;
    ++Context_->OpenRecordings;
  }

  ~Recording() override;

  [[nodiscard]] Result<void> Initialize();
  [[nodiscard]] Result<void> End();
  void CommitImages() noexcept;
  void CommitSubmission() noexcept;
  void CommitPresent() noexcept;
  void BindPresentation(VkSubmitInfo2 &submit,
                        VkSemaphoreSubmitInfo &wait,
                        VkSemaphoreSubmitInfo &signal) const noexcept;

  [[nodiscard]] const std::shared_ptr<Context> &Owner() const noexcept { return Context_; }

  [[nodiscard]] VkCommandBuffer Handle() const noexcept { return Commands_; }

  [[nodiscard]] GpuTimingResults Timings() const { return Timings_.Read(); }

  [[nodiscard]] Result<SubmissionGpuTiming> SubmissionTime() const {
    return Timings_.ReadSubmission();
  }

  [[nodiscard]] const BindingWork &BindingCosts() const noexcept { return Storage_.Work(); }

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
  [[nodiscard]] Result<void> BeginRenderPass(const RenderPassDescription &description) override;
  [[nodiscard]] Result<void> Draw(const DrawDescription &description) override;
  [[nodiscard]] Result<void> DrawIndexed(const IndexedDrawDescription &description) override;
  [[nodiscard]] Result<void>
  DrawIndexedIndirect(const IndirectDrawDescription &description) override;
  [[nodiscard]] Result<void> EndRenderPass() override;
  [[nodiscard]] Result<void> Present(std::unique_ptr<PresentFrame> frame) override;
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

  [[nodiscard]] Result<TrackedImage *> TrackImage(const std::shared_ptr<ImageState> &image,
                                                  TextureView view);
  [[nodiscard]] Result<void> FinalizePresentation();
  [[nodiscard]] Result<void> Hold(const std::shared_ptr<BufferState> &buffer);
  [[nodiscard]] Result<void>
  UseImage(const std::shared_ptr<ImageState> &image, TextureView view, ImageAccess access);
  [[nodiscard]] Result<VkDescriptorImageInfo> BindImage(const ShaderBinding &declared,
                                                        const Binding &binding,
                                                        const PipelineState &pipeline,
                                                        VkPipelineStageFlags2 stages);

  void Use(const BufferState &buffer, ByteRange range, BufferAccess access);
  [[nodiscard]] Result<std::vector<VkDescriptorSet>>
  BindResources(const PipelineState &pipeline,
                std::span<const ShaderBinding> layout,
                std::span<const Binding> bindings,
                VkPipelineStageFlags2 stages);
  [[nodiscard]] Result<VkDescriptorBufferInfo>
  BindBuffer(const ShaderBinding &declared, const Binding &binding, VkPipelineStageFlags2 stages);

  [[nodiscard]] Result<RenderAttachment>
  PrepareAttachment(TextureView view, Load begin, Store end, bool depth);
  [[nodiscard]] Result<DrawPacket> PrepareDraw(const DrawState &description);
  [[nodiscard]] Result<void> BindVertices(DrawPacket &packet,
                                          std::span<const VertexBuffer> vertices);
  [[nodiscard]] Result<void> AppendDraw(DrawPacket packet);
  [[nodiscard]] Result<void> BindIndices(DrawPacket &packet, const IndexBuffer &indices);
  [[nodiscard]] Result<void> CheckFeedback(std::span<const Binding> bindings) const;
  void EncodeDraw(const DrawPacket &packet);

  std::unique_ptr<PresentFrame> Frame_;
  std::shared_ptr<PresentLease> Presented_;
  std::optional<RenderState> Render_;
  std::shared_ptr<Context> Context_;
  TimingQueries Timings_;
  VkCommandPool Pool_ = VK_NULL_HANDLE;
  VkCommandBuffer Commands_ = VK_NULL_HANDLE;
  bool Finished_ = false;
  bool Submitted_ = false;
  std::vector<TrackedImage> Images_;
  std::vector<std::shared_ptr<SamplerState>> Samplers_;
  std::vector<std::shared_ptr<BufferState>> Buffers_;
  std::vector<std::shared_ptr<PipelineState>> Pipelines_;
  BindingStorage Storage_;
};

}
#endif
#endif
