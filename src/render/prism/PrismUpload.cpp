#include "PrismUpload.h"
#include "UploadQueue.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<PrismBuffers> UploadPrismBuffers(Native::Device &device,
                                                UploadQueue *queue,
                                                std::span<const std::byte> samples,
                                                std::span<const std::byte> patches) {
  const std::array sources{samples, patches};
  uint64_t bytes = 0;
  for (const auto source : sources) {
    if (source.size() > device.Limits().MaxStorageBufferBytes ||
        source.size() > std::numeric_limits<uint32_t>::max() ||
        source.size() > std::numeric_limits<size_t>::max() - bytes) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Unsupported,
                                           .Message = "prism storage exceeds device limits"});
    }
    bytes += source.size();
  }
  PrismBuffers result;
  if (bytes == 0) { return result; }
  auto commands = BeginUpload(device, queue);
  if (!commands) { return std::unexpected(commands.error()); }
  auto staging = device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload},
      "prism staging");
  if (!staging) { return std::unexpected(staging.error()); }
  for (size_t at = 0; at < sources.size(); ++at) {
    if (sources[at].empty()) { continue; }
    auto buffer =
        device.CreateBuffer({.Bytes = sources[at].size(),
                             .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource |
                                    Native::BufferUse::CopyDestination,
                             .Placement = Native::Memory::Device},
                            at == 0 ? "prism samples" : "prism patches");
    if (!buffer) { return std::unexpected(buffer.error()); }
    result[at] = std::move(*buffer);
  }
  auto mapped = device.MapWrite(**staging, {.Bytes = bytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  size_t offset = 0;
  for (const auto source : sources) {
    if (!source.empty()) { std::memcpy(mapped->data() + offset, source.data(), source.size()); }
    offset += source.size();
  }
  if (auto unmapped = device.Unmap(**staging); !unmapped) {
    return std::unexpected(unmapped.error());
  }
  uint64_t from = 0;
  for (size_t at = 0; at < sources.size(); ++at) {
    if (sources[at].empty()) { continue; }
    if (auto copied = (*commands)->Copy(
            **staging, *result[at], {.SourceOffset = from, .Bytes = sources[at].size()});
        !copied) {
      return std::unexpected(copied.error());
    }
    from += sources[at].size();
  }
  auto submitted = SubmitUpload(device, queue, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  return result;
}
}
