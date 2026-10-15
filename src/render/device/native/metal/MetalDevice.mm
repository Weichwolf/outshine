#include "MetalPresentation.h"
#include <atomic>
#include <limits>

namespace outshine::Render::Native::Metal {

MetalDevice::MetalDevice(uint64_t identity, id<MTLDevice> device, id<MTLCommandQueue> queue)
    : Identity_(identity), Native_(device), Queue_(queue) {
  constexpr uint32_t colourTargets = 8;
  constexpr uint32_t maxUniformBytes = 64 * 1024;
  constexpr uint32_t baselineInvocations = 256;
  Limits_.Name = device.name.UTF8String;
  Limits_.MaxColourAttachments = colourTargets;
  constexpr uint32_t textureExtent = 16384;
  constexpr uint32_t textureLayers = 2048;
  Limits_.MaxTextureExtent = textureExtent;
  Limits_.MaxTextureLayers = textureLayers;
  Limits_.StorageBufferAlignment = 4;
  Limits_.UniformBufferAlignment = baselineInvocations;
  Limits_.MaxStorageBufferBytes = device.maxBufferLength;
  Limits_.MaxUniformBufferBytes = maxUniformBytes;
  const auto size = device.maxThreadsPerThreadgroup;
  Limits_.MaxWorkgroupSize = {static_cast<uint32_t>(size.width),
                              static_cast<uint32_t>(size.height),
                              static_cast<uint32_t>(size.depth)};
  Limits_.MaxWorkgroupCount.fill(std::numeric_limits<uint32_t>::max());
  Limits_.MaxWorkgroupInvocations = baselineInvocations;
}

MetalDevice::~MetalDevice() {
  if (!Pending_.empty()) { [Pending_.back()->Commands waitUntilCompleted]; }
}

Result<std::unique_ptr<Buffer>> MetalDevice::CreateBuffer(BufferDescription description,
                                                          std::string_view label) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    constexpr uint32_t knownUses = (1u << 6u) - 1;
    const auto uses = static_cast<uint32_t>(description.Use);
    if (description.Bytes == 0 || description.Bytes > Native_.maxBufferLength || uses == 0 ||
        (uses & ~knownUses) != 0) {
      return Failure(ErrorKind::Invalid, "invalid Metal buffer description");
    }
    MTLResourceOptions memory;
    switch (description.Placement) {
      case Memory::Device: memory = MTLResourceStorageModePrivate; break;
      case Memory::Upload:
      case Memory::Readback: memory = MTLResourceStorageModeShared; break;
      default: return Failure(ErrorKind::Invalid, "invalid Metal buffer placement");
    }
    const id<MTLBuffer> buffer = [Native_ newBufferWithLength:description.Bytes options:memory];
    if (buffer == nil) { return Failure(ErrorKind::OutOfMemory, "Metal buffer allocation failed"); }
    buffer.label = Text(label);
    auto state = std::make_shared<BufferState>();
    state->Native = buffer;
    state->Device = Identity_;
    return std::make_unique<MetalBuffer>(description, std::move(state));
  }
}

Result<std::span<std::byte>>
MetalDevice::Map(Buffer &buffer, ByteRange range, Memory memory) const {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  const auto *const native = dynamic_cast<MetalBuffer *>(&buffer);
  if (native == nullptr || native->State->Device != Identity_ ||
      buffer.Description().Placement != memory || !Fits(range, buffer.Description().Bytes)) {
    return Failure(ErrorKind::Invalid, "invalid Metal host buffer range or ownership");
  }
  const std::scoped_lock lock(native->State->Mutex);
  if (native->State->Uses != 0 || native->State->Mapped) {
    return Failure(ErrorKind::Busy, "Metal buffer is recorded, in flight or already mapped");
  }
  native->State->Mapped = true;
  return std::span(static_cast<std::byte *>(native->State->Native.contents) + range.Offset,
                   static_cast<size_t>(range.Bytes));
}

Result<std::span<std::byte>> MetalDevice::MapWrite(Buffer &buffer, ByteRange range) {
  return Map(buffer, range, Memory::Upload);
}

Result<std::span<const std::byte>> MetalDevice::MapRead(Buffer &buffer, ByteRange range) {
  return Map(buffer, range, Memory::Readback);
}

Result<void> MetalDevice::Unmap(Buffer &buffer) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  const auto *const native = dynamic_cast<MetalBuffer *>(&buffer);
  if (native == nullptr || native->State->Device != Identity_) {
    return Failure(ErrorKind::Invalid, "Metal buffer belongs to a different device");
  }
  const std::scoped_lock lock(native->State->Mutex);
  if (!native->State->Mapped) { return Failure(ErrorKind::Invalid, "Metal buffer is not mapped"); }
  native->State->Mapped = false;
  return {};
}

Result<std::unique_ptr<Commands>> MetalDevice::Begin() {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    Collect();
    if (Failed_) { return std::unexpected(*Failed_); }
    constexpr uint32_t maxRecordings = 8;
    if (Active_->fetch_add(1) >= maxRecordings) {
      Active_->fetch_sub(1);
      return Failure(ErrorKind::Busy, "Metal command recording capacity is occupied");
    }
    auto permit = std::make_shared<CommandPermit>(Active_);
    const id<MTLCommandBuffer> commands = [Queue_ commandBuffer];
    if (commands == nil) {
      return Failure(ErrorKind::Driver, "Metal command buffer allocation failed");
    }
    return std::make_unique<MetalCommands>(Identity_, commands, std::move(permit));
  }
}

}

namespace outshine::Render::Native {

Result<std::unique_ptr<Device>> CreateDevice(DeviceOptions options) {
  @autoreleasepool {
    static std::atomic<uint64_t> identity{1};
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return Metal::Failure(ErrorKind::Unsupported, "no native Metal device"); }
    if (options.Validation) {
      NSString *const validation = NSProcessInfo.processInfo.environment[@"MTL_DEBUG_LAYER"];
      if (![validation isEqualToString:@"1"]) {
        return Metal::Failure(ErrorKind::Unsupported,
                              "enable MTL_DEBUG_LAYER=1 before requesting validation");
      }
    }
    const id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) { return Metal::Failure(ErrorKind::Driver, "Metal queue creation failed"); }
    auto result = std::make_unique<Metal::MetalDevice>(identity.fetch_add(1), device, queue);
    if (options.Window != nullptr) {
      const auto attached = result->Attach(options.Window);
      if (!attached) { return std::unexpected(attached.error()); }
    }
    return result;
  }
}

}
