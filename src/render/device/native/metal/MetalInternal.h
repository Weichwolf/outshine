#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALINTERNAL_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALINTERNAL_H

#include "../Device.h"
#include "MetalShader.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

namespace outshine::Render::Native::Metal {

[[nodiscard]] inline std::unexpected<Error> Failure(ErrorKind kind, std::string message) {
  return std::unexpected(Error{.Kind = kind, .Message = std::move(message)});
}

[[nodiscard]] inline NSString *Text(std::string_view text) {
  return [[NSString alloc] initWithBytes:text.data()
                                  length:text.size()
                                encoding:NSUTF8StringEncoding];
}

struct BufferState {
  id<MTLBuffer> Native;
  uint64_t Device;
  std::mutex Mutex;
  size_t Uses = 0;
  bool Mapped = false;
};

class MetalBuffer final : public Buffer {
public:
  MetalBuffer(BufferDescription description, std::shared_ptr<BufferState> state)
      : Buffer(description), State(std::move(state)) {}

  std::shared_ptr<BufferState> State;
};

class MetalTexture final : public Texture {
public:
  MetalTexture(TextureDescription description, id<MTLTexture> texture, uint64_t device)
      : Texture(description), Native(texture), Device(device) {}

  id<MTLTexture> Native;
  uint64_t Device;
};

class MetalSampler final : public Sampler {
public:
  MetalSampler(id<MTLSamplerState> sampler, uint64_t device, bool filtered)
      : Native(sampler), Device(device), Filtered(filtered) {}

  id<MTLSamplerState> Native;
  uint64_t Device;
  bool Filtered;
};

struct TexelDescription {
  MTLPixelFormat Native;
  uint32_t Bytes;
};

[[nodiscard]] std::optional<TexelDescription> Texels(Format format);
[[nodiscard]] Result<id<MTLTexture>> View(TextureView view, uint64_t device);
[[nodiscard]] bool Inside(TextureRegion region, TextureDescription description);

struct Reservation {
  explicit Reservation(std::shared_ptr<BufferState> state) : State(std::move(state)) {}

  Reservation(Reservation &&) noexcept = default;
  Reservation(const Reservation &) = delete;
  Reservation &operator=(const Reservation &) = delete;
  Reservation &operator=(Reservation &&) = delete;
  ~Reservation();
  std::shared_ptr<BufferState> State;
};

struct PipelineState {
  id<MTLComputePipelineState> Native;
  Shader Translated;
  uint64_t Device;
};

struct DispatchArgument {
  const ResourceBinding *Layout = nullptr;
  const StorageBuffer *Storage = nullptr;
  const Uniform *Constants = nullptr;
  std::shared_ptr<BufferState> Buffer;
  id<MTLTexture> Texture = nil;
  id<MTLSamplerState> Sampler = nil;
  id<MTLResource> Resource = nil;
};

[[nodiscard]] Result<std::vector<DispatchArgument>>
Arguments(const PipelineState &pipeline, std::span<const Binding> bindings, uint64_t device);
[[nodiscard]] Result<void> Bind(id<MTLComputeCommandEncoder> encoder,
                                const Shader &shader,
                                const std::vector<DispatchArgument> &arguments);

class MetalPipeline final : public ComputePipeline {
public:
  explicit MetalPipeline(std::shared_ptr<PipelineState> state)
      : ComputePipeline(state->Translated.Layout), State(std::move(state)) {}

  std::shared_ptr<PipelineState> State;
};

struct Submission {
  uint64_t Serial = 0;
  id<MTLCommandBuffer> Commands;
  std::mutex Mutex;
  std::condition_variable Changed;
  bool Done = false;
  std::optional<Error> Failed;
};

struct CommandPermit {
  explicit CommandPermit(std::shared_ptr<std::atomic<uint32_t>> count) : Count(std::move(count)) {}

  CommandPermit(const CommandPermit &) = delete;
  CommandPermit &operator=(const CommandPermit &) = delete;

  ~CommandPermit() { Count->fetch_sub(1); }

  std::shared_ptr<std::atomic<uint32_t>> Count;
};

struct SubmissionResources {
  std::vector<Reservation> Buffers;
  std::shared_ptr<CommandPermit> Permit;
};

struct TrackedResource {
  id<MTLResource> Resource;
  Access Usage;
};

class MetalCommands final : public Commands {
public:
  MetalCommands(uint64_t device,
                id<MTLCommandBuffer> commands,
                std::shared_ptr<CommandPermit> permit)
      : Device(device), Native(commands), Permit(std::move(permit)) {}

  ~MetalCommands() override;

  Result<void> Copy(const Buffer &source, Buffer &destination, BufferCopy region) override;
  Result<void> Copy(const Buffer &source, Texture &destination, TextureTransfer region) override;
  Result<void> Copy(const Texture &source, Buffer &destination, TextureTransfer region) override;
  Result<void> Copy(const Texture &source,
                    TextureRegion sourceRegion,
                    Texture &destination,
                    TextureRegion destinationRegion) override;
  Result<void> Dispatch(const ComputePipeline &pipeline,
                        std::span<const Binding> bindings,
                        Workgroups groups) override;
  void BeginLabel(std::string_view name) override;
  void EndLabel() override;

private:
  friend class MetalDevice;
  [[nodiscard]] Result<void> Hold(const std::shared_ptr<BufferState> &state);
  void Finish();

  uint64_t Device;
  id<MTLCommandBuffer> Native;
  std::vector<Reservation> Held;
  std::shared_ptr<CommandPermit> Permit;
  id<MTLComputeCommandEncoder> Compute = nil;
  std::vector<TrackedResource> Accesses;

  void UpdateAccesses(std::span<const DispatchArgument> arguments);

  [[nodiscard]] bool OnOwner() const noexcept { return Owner_ == std::this_thread::get_id(); }

  const std::thread::id Owner_ = std::this_thread::get_id();
};

class MetalDevice final : public Device {
public:
  MetalDevice(uint64_t identity, id<MTLDevice> device, id<MTLCommandQueue> queue);
  ~MetalDevice() override;

  [[nodiscard]] const Capabilities &Limits() const noexcept override { return Limits_; }

  [[nodiscard]] bool Supports(Format format, UseMask<TextureUse> use, bool filtered) const override;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label) override;
  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label) override;
  Result<std::unique_ptr<Sampler>> CreateSampler(SamplerDescription description,
                                                 std::string_view label) override;
  Result<std::unique_ptr<ComputePipeline>> CreateComputePipeline(std::span<const uint32_t> spirv,
                                                                 ComputeOptions settings) override;
  Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override;
  Result<std::span<const std::byte>> MapRead(Buffer &buffer, ByteRange range) override;
  Result<void> Unmap(Buffer &buffer) override;
  Result<std::unique_ptr<Commands>> Begin() override;
  Result<Completion> Submit(std::unique_ptr<Commands> commands) override;
  Result<CompletionStatus> Poll(Completion completion) override;
  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override;

private:
  [[nodiscard]] bool OnOwner() const noexcept { return Owner_ == std::this_thread::get_id(); }

  const std::thread::id Owner_ = std::this_thread::get_id();
  [[nodiscard]] Result<std::span<std::byte>>
  Map(Buffer &buffer, ByteRange range, Memory memory) const;
  void Collect();

  uint64_t Identity_, NextSerial_ = 1, Retired_ = 0;
  id<MTLDevice> Native_;
  id<MTLCommandQueue> Queue_;
  Capabilities Limits_;
  std::deque<std::shared_ptr<Submission>> Pending_;
  std::optional<Error> Failed_;
  std::shared_ptr<std::atomic<uint32_t>> Active_ = std::make_shared<std::atomic<uint32_t>>(0);
};

template <typename Flag> bool Has(UseMask<Flag> mask, Flag flag) {
  return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0;
}

[[nodiscard]] inline bool Fits(ByteRange range, uint64_t bytes) {
  return range.Bytes > 0 && range.Offset <= bytes && range.Bytes <= bytes - range.Offset;
}

}
#endif
