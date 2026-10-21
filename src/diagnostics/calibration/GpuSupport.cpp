#include "GpuSupport.h"
#include <cstring>
#include <memory>
#include <span>
#include <cstddef>
#include <expected>
#include <utility>

namespace outshine::Diagnostics::Calibration {
Native::Result<void> Finish(Native::Device &device, std::unique_ptr<Native::Commands> commands) {
  const auto submitted = device.Submit(std::move(commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  const auto waited = device.Wait(*submitted, kGpuWait);
  if (!waited) { return std::unexpected(waited.error()); }
  if (*waited != Native::CompletionStatus::Ready) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "calibration transfer exceeded five seconds"});
  }
  return {};
}

Native::Result<void> Upload(Native::Device &device,
                            Native::Commands &commands,
                            Native::Buffer &destination,
                            std::span<const std::byte> bytes) {
  auto staging = device.CreateBuffer({.Bytes = bytes.size(),
                                      .Use = Native::BufferUse::CopySource,
                                      .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = device.MapWrite(**staging, {.Bytes = bytes.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), bytes.data(), bytes.size());
  const auto unmapped = device.Unmap(**staging);
  if (!unmapped) { return unmapped; }
  return commands.Copy(**staging, destination, {.Bytes = bytes.size()});
}

Native::Result<bool> VerifyBuffer(Native::Device &device,
                                  const Native::Buffer &source,
                                  Native::Buffer &readback,
                                  std::span<const std::byte> expected) {
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  const auto copied = (*commands)->Copy(source, readback, {.Bytes = expected.size()});
  if (!copied) { return std::unexpected(copied.error()); }
  const auto finished = Finish(device, std::move(*commands));
  if (!finished) { return std::unexpected(finished.error()); }
  const auto mapped = device.MapRead(readback, {.Bytes = expected.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  const bool same = std::memcmp(mapped->data(), expected.data(), expected.size()) == 0;
  const auto unmapped = device.Unmap(readback);
  if (!unmapped) { return std::unexpected(unmapped.error()); }
  return same;
}
}
