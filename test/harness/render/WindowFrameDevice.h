#ifndef OUTSHINE_TEST_RENDER_WINDOWFRAMEDEVICE_H
#define OUTSHINE_TEST_RENDER_WINDOWFRAMEDEVICE_H

#include "NativeCommandsProxy.h"
#include "NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace outshine::Test::Gpu::WindowFrame {
namespace N = outshine::Render::Native;
inline unsigned swapchainCalls = 0;
inline unsigned forbiddenReads = 0;
inline unsigned downloadFailures = 0;
inline unsigned commandAcquires = 0;
enum class DownloadFailure { None, Acquire, Copy, Map, Wait };
inline DownloadFailure downloadFailure = DownloadFailure::None;
inline bool downloadPending = false;
inline bool skipSwapchain = false;
inline bool failSwapchain = false;
inline std::unordered_set<const N::Buffer *> downloads;
inline std::unordered_set<const N::Commands *> downloadCommands;
inline std::set<std::pair<uint64_t, uint64_t>> downloadCompletions;

inline N::Error Injected(const char *message) {
  downloadFailure = DownloadFailure::None;
  ++downloadFailures;
  return {.Kind = N::ErrorKind::Driver, .Message = message};
}

class DownloadBuffer final : public N::Buffer {
public:
  explicit DownloadBuffer(std::unique_ptr<N::Buffer> backing)
      : N::Buffer(backing->Description()), Backing_(std::move(backing)) {
    downloads.insert(this);
  }

  ~DownloadBuffer() override { downloads.erase(this); }

  [[nodiscard]] N::Buffer &Actual() const { return *Backing_; }

private:
  std::unique_ptr<N::Buffer> Backing_;
};

inline N::Buffer &Actual(N::Buffer &buffer) {
  auto *download = dynamic_cast<DownloadBuffer *>(&buffer);
  return download == nullptr ? buffer : download->Actual();
}

class FrameCommands final : public NativeCommandsProxy {
public:
  FrameCommands(std::unique_ptr<N::Commands> commands, bool download)
      : NativeCommandsProxy(std::move(commands)), Download_(download) {
    if (Download_) { downloadCommands.insert(this); }
  }

  ~FrameCommands() override { downloadCommands.erase(this); }

  [[nodiscard]] bool IsDownload() const { return Download_; }

  using NativeCommandsProxy::Copy;

  [[nodiscard]] N::Result<void>
  Copy(const N::Texture &source, N::Buffer &destination, N::TextureTransfer region) override {
    if ((static_cast<uint32_t>(source.Description().Use) &
         static_cast<uint32_t>(N::TextureUse::CopySource)) == 0) {
      ++forbiddenReads;
      return std::unexpected(N::Error{.Kind = N::ErrorKind::Invalid,
                                      .Message = "readback attempted a write-only texture"});
    }
    if (Download_ && downloadFailure == DownloadFailure::Copy) {
      return std::unexpected(Injected("injected download copy failure"));
    }
    return NativeCommandsProxy::Copy(source, Actual(destination), region);
  }

private:
  bool Download_;
};

class FrameDevice final : public NativeDeviceProxy {
public:
  explicit FrameDevice(std::unique_ptr<N::Device> device)
      : NativeDeviceProxy(*device), Device_(std::move(device)) {}

  [[nodiscard]] static N::Result<std::unique_ptr<N::Device>> Create(void *,
                                                                    N::DeviceOptions options) {
    auto device = N::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<FrameDevice>(std::move(*device));
  }

  [[nodiscard]] N::Result<std::unique_ptr<N::Buffer>>
  CreateBuffer(N::BufferDescription description, std::string_view label) override {
    auto buffer = Backing().CreateBuffer(description, label);
    if (!buffer || description.Placement != N::Memory::Readback) { return buffer; }
    downloadPending = true;
    return std::make_unique<DownloadBuffer>(std::move(*buffer));
  }

  [[nodiscard]] N::Result<std::unique_ptr<N::Commands>> Begin() override {
    ++commandAcquires;
    const bool download = std::exchange(downloadPending, false);
    if (download && downloadFailure == DownloadFailure::Acquire) {
      return std::unexpected(Injected("injected download command acquisition failure"));
    }
    auto commands = Backing().Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<FrameCommands>(std::move(*commands), download);
  }

  [[nodiscard]] N::Result<N::Completion> Submit(std::unique_ptr<N::Commands> commands) override {
    auto *frame = dynamic_cast<FrameCommands *>(commands.get());
    if (frame == nullptr) { return Backing().Submit(std::move(commands)); }
    const bool download = frame->IsDownload();
    auto submitted = Backing().Submit(frame->Take());
    if (submitted && download) {
      downloadCompletions.insert({submitted->Device, submitted->Serial});
    }
    return submitted;
  }

  [[nodiscard]] N::Result<std::span<const std::byte>> MapRead(N::Buffer &buffer,
                                                              N::ByteRange range) override {
    if (downloads.contains(&buffer) && downloadFailure == DownloadFailure::Map) {
      return std::unexpected(Injected("injected download mapping failure"));
    }
    return Backing().MapRead(Actual(buffer), range);
  }

  [[nodiscard]] N::Result<void> Unmap(N::Buffer &buffer) override {
    return Backing().Unmap(Actual(buffer));
  }

  [[nodiscard]] N::Result<N::CompletionStatus> Wait(N::Completion completion,
                                                    std::chrono::nanoseconds budget) override {
    const bool download = downloadCompletions.erase({completion.Device, completion.Serial}) != 0;
    if (download && downloadFailure == DownloadFailure::Wait) {
      return std::unexpected(Injected("injected download fence wait failure"));
    }
    return Backing().Wait(completion, budget);
  }

  [[nodiscard]] N::Result<std::unique_ptr<N::PresentFrame>> AcquirePresentFrame() override {
    ++swapchainCalls;
    if (std::exchange(failSwapchain, false)) {
      return std::unexpected(N::Error{.Kind = N::ErrorKind::Driver,
                                      .Message = "injected swapchain acquisition failure"});
    }
    if (std::exchange(skipSwapchain, false)) { return std::unique_ptr<N::PresentFrame>{}; }
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < until) {
      SDL_PumpEvents();
      auto frame = Backing().AcquirePresentFrame();
      if (!frame || *frame) { return frame; }
      SDL_Delay(1);
    }
    return std::unexpected(
        N::Error{.Kind = N::ErrorKind::Driver,
                 .Message = "test window supplied no frame within its bounded wait"});
  }

private:
  std::unique_ptr<N::Device> Device_;
};
}
#endif
