#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_BINDINGSTORAGE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_BINDINGSTORAGE_H
#ifdef __linux__
#include "Buffer.h"
#include "VulkanImage.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace outshine::Render::Native::Vulkan {
struct BindingWork {
  uint64_t SetRequests = 0;
  uint64_t SetHits = 0;
  uint64_t Sets = 0;
  uint64_t Pools = 0;
  uint64_t UniformRequests = 0;
  uint64_t UniformHits = 0;
  uint64_t UniformUploadBytes = 0;
  uint64_t UniformSnapshotBytes = 0;
  uint64_t UniformCapacityBytes = 0;
  uint64_t UniformPages = 0;
  uint64_t ViewRequests = 0;
  uint64_t ViewHits = 0;
  uint64_t Views = 0;
};

struct UniformSlice {
  std::shared_ptr<BufferState> Buffer;
  ByteRange Range;
};

struct DescriptorResource {
  uint32_t Set = 0;
  uint32_t Slot = 0;
  VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  VkDescriptorBufferInfo Buffer{};
  VkDescriptorImageInfo Image{};
};

class BindingStorage {
public:
  static constexpr uint64_t kUniformPageBytes = uint64_t{64} * 1024u;
  static constexpr uint64_t kUniformCapacityMost = uint64_t{16} * 1024u * 1024u;
  static constexpr uint64_t kSetsMost = uint64_t{16} * 1024u;
  static constexpr uint64_t kPoolsMost = 128;
  static constexpr uint64_t kViewsMost = uint64_t{16} * 1024u;

  explicit BindingStorage(std::shared_ptr<Context> context) : Context_(std::move(context)) {}

  ~BindingStorage();
  BindingStorage(const BindingStorage &) = delete;
  BindingStorage &operator=(const BindingStorage &) = delete;

  [[nodiscard]] Result<UniformSlice> UploadUniform(std::span<const std::byte> bytes);
  [[nodiscard]] Result<VkImageView> ImageView(const ImageState &image, TextureView view);
  [[nodiscard]] Result<VkDescriptorSet>
  DescriptorSet(VkDescriptorSetLayout layout, std::span<const DescriptorResource> resources);
  [[nodiscard]] Result<void> Seal();

  [[nodiscard]] const BindingWork &Work() const noexcept { return Work_; }

private:
  struct Page {
    std::shared_ptr<BufferState> Buffer;
    std::span<std::byte> Mapped;
    uint64_t Used = 0;
  };

  struct ByteHash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(std::string_view bytes) const noexcept;
  };

  using DescriptorWords = std::array<uint64_t, 6>;
  using ViewKey = std::array<uint64_t, 5>;

  struct SetKey {
    VkDescriptorSetLayout Layout = VK_NULL_HANDLE;
    std::vector<DescriptorWords> Resources;
    [[nodiscard]] bool operator==(const SetKey &) const = default;
  };

  struct SetHash {
    [[nodiscard]] size_t operator()(const SetKey &key) const noexcept;
  };

  struct ViewHash {
    [[nodiscard]] size_t operator()(const ViewKey &key) const noexcept;
  };

  [[nodiscard]] Result<void> NewUniformPage(uint64_t bytes);
  [[nodiscard]] Result<void> NewPool(std::span<const DescriptorResource> resources);
  [[nodiscard]] Result<VkDescriptorSet> AllocateSet(VkDescriptorSetLayout layout,
                                                    std::span<const DescriptorResource> resources);
  void WriteSet(VkDescriptorSet set, std::span<const DescriptorResource> resources);
  static DescriptorWords Identity(const DescriptorResource &resource) noexcept;

  std::shared_ptr<Context> Context_;
  BindingWork Work_;
  std::vector<Page> UniformPages_;
  std::unordered_map<std::string, UniformSlice, ByteHash, std::equal_to<>> Uniforms_;
  std::unordered_map<SetKey, VkDescriptorSet, SetHash> Sets_;
  std::unordered_map<ViewKey, VkImageView, ViewHash> Views_;
  std::vector<VkDescriptorPool> Pools_;
  SetKey Scratch_;
  bool Sealed_ = false;
};
}
#endif
#endif
