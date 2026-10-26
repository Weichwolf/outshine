#ifdef __linux__
#include "BindingStorage.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <type_traits>

namespace outshine::Render::Native::Vulkan {
namespace {
template <class Handle> uint64_t HandleBits(Handle handle) noexcept {
  if constexpr (std::is_pointer_v<Handle>) {
    return reinterpret_cast<uintptr_t>(handle);
  } else {
    return handle;
  }
}
}

BindingStorage::~BindingStorage() {
  for (const auto &page : UniformPages_) { --page.Buffer->RecordedUses; }
  for (auto *const pool : Pools_) { vkDestroyDescriptorPool(Context_->Logical, pool, nullptr); }
  for (const auto &[key, view] : Views_) { vkDestroyImageView(Context_->Logical, view, nullptr); }
}

size_t BindingStorage::SetHash::operator()(const SetKey &key) const noexcept {
  auto hash = std::hash<VkDescriptorSetLayout>{}(key.Layout);
  for (const auto &resource : key.Resources) {
    for (const auto word : resource) { hash = std::rotl(hash, 1) ^ std::hash<uint64_t>{}(word); }
  }
  return hash;
}

size_t BindingStorage::ViewHash::operator()(const ViewKey &key) const noexcept {
  size_t hash = 0;
  for (const auto word : key) { hash = std::rotl(hash, 1) ^ std::hash<uint64_t>{}(word); }
  return hash;
}

BindingStorage::DescriptorWords
BindingStorage::Identity(const DescriptorResource &resource) noexcept {
  if (resource.Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
      resource.Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
    return {resource.Slot,
            static_cast<uint64_t>(resource.Type),
            HandleBits(resource.Buffer.buffer),
            resource.Buffer.offset,
            resource.Buffer.range,
            0};
  }
  return {resource.Slot,
          static_cast<uint64_t>(resource.Type),
          HandleBits(resource.Image.sampler),
          HandleBits(resource.Image.imageView),
          static_cast<uint64_t>(resource.Image.imageLayout),
          0};
}

Result<VkImageView> BindingStorage::ImageView(const ImageState &image, TextureView view) {
  ++Work_.ViewRequests;
  const ViewKey key{
      reinterpret_cast<uintptr_t>(&image), view.FirstMip, view.Mips, view.FirstLayer, view.Layers};
  if (const auto found = Views_.find(key); found != Views_.end()) {
    ++Work_.ViewHits;
    return found->second;
  }
  if (Work_.Views == kViewsMost) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "Vulkan recording image-view capacity exhausted"});
  }
  auto made = CreateView(image, view);
  if (!made) { return std::unexpected(made.error()); }
  Views_.emplace(key, *made);
  ++Work_.Views;
  return *made;
}

Result<VkDescriptorSet>
BindingStorage::DescriptorSet(VkDescriptorSetLayout layout,
                              std::span<const DescriptorResource> resources) {
  ++Work_.SetRequests;
  Scratch_.Layout = layout;
  Scratch_.Resources.clear();
  for (const auto &resource : resources) { Scratch_.Resources.push_back(Identity(resource)); }
  if (const auto found = Sets_.find(Scratch_); found != Sets_.end()) {
    ++Work_.SetHits;
    return found->second;
  }
  if (Work_.Sets == kSetsMost) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "Vulkan recording descriptor-set capacity exhausted"});
  }
  auto set = AllocateSet(layout, resources);
  if (!set) { return std::unexpected(set.error()); }
  WriteSet(*set, resources);
  Sets_.emplace(Scratch_, *set);
  ++Work_.Sets;
  return *set;
}
}
#endif
