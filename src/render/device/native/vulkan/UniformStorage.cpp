#ifdef __linux__
#include "BindingStorage.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Render::Native::Vulkan {
size_t BindingStorage::ByteHash::operator()(std::string_view bytes) const noexcept {
  return std::hash<std::string_view>{}(bytes);
}

Result<void> BindingStorage::NewUniformPage(uint64_t bytes) {
  const uint64_t capacity = std::max(kUniformPageBytes, bytes);
  if (capacity > kUniformCapacityMost - Work_.UniformCapacityBytes) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Busy, .Message = "Vulkan recording uniform capacity exhausted"});
  }
  auto buffer =
      AllocateBuffer(Context_,
                     {.Bytes = capacity, .Use = BufferUse::None, .Placement = Memory::Upload},
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  if (!buffer) { return std::unexpected(buffer.error()); }
  auto mapped = MapWrite(**buffer, {.Bytes = capacity});
  if (!mapped) { return std::unexpected(mapped.error()); }
  ++(*buffer)->RecordedUses;
  UniformPages_.push_back({.Buffer = std::move(*buffer), .Mapped = *mapped});
  Work_.UniformCapacityBytes += capacity;
  ++Work_.UniformPages;
  return {};
}

Result<UniformSlice> BindingStorage::UploadUniform(std::span<const std::byte> bytes) {
  if (Sealed_ || bytes.empty() || bytes.size() > Context_->Limits.MaxUniformBufferBytes) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid recording uniform snapshot"});
  }
  ++Work_.UniformRequests;
  const std::string_view key(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  if (const auto found = Uniforms_.find(key); found != Uniforms_.end()) {
    ++Work_.UniformHits;
    return found->second;
  }
  const auto alignment = Context_->Properties.limits.minUniformBufferOffsetAlignment;
  const auto nextOffset = [alignment](uint64_t at) {
    return ((at + alignment - 1u) / alignment) * alignment;
  };
  uint64_t offset = UniformPages_.empty() ? 0 : nextOffset(UniformPages_.back().Used);
  if (UniformPages_.empty() || offset + bytes.size() > UniformPages_.back().Mapped.size()) {
    const auto fresh = NewUniformPage(bytes.size());
    if (!fresh) { return std::unexpected(fresh.error()); }
    offset = 0;
  }
  auto &page = UniformPages_.back();
  std::memcpy(page.Mapped.data() + offset, bytes.data(), bytes.size());
  page.Used = offset + bytes.size();
  UniformSlice slice{.Buffer = page.Buffer, .Range = {.Offset = offset, .Bytes = bytes.size()}};
  Uniforms_.emplace(std::string(key), slice);
  Work_.UniformUploadBytes += bytes.size();
  Work_.UniformSnapshotBytes += bytes.size();
  return slice;
}

Result<void> BindingStorage::Seal() {
  if (Sealed_) { return {}; }
  for (auto &page : UniformPages_) {
    if (page.Buffer->Mapped == nullptr) { continue; }
    const auto closed = Unmap(*page.Buffer);
    if (!closed) { return closed; }
    page.Mapped = {};
  }
  Sealed_ = true;
  return {};
}
}
#endif
