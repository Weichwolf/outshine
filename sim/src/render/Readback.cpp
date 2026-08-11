#include "Readback.h"

#include "Log.h"

namespace outshine::Render {
namespace {

std::string SvToStr(wgpu::StringView v) {
  return v.data ? std::string(v.data, v.length) : std::string();
}

}  // namespace

void Readback::Map(uint64_t bytes) {
  Bytes = bytes;
  Done = false;
  Ok = false;
  /* AllowProcessEvents and not AllowSpontaneous: the callback then fires inside Poll and nowhere
   * else, so a readback cannot land in the middle of an encoder. */
  Staging.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::AllowProcessEvents,
                   [this](wgpu::MapAsyncStatus st, wgpu::StringView msg) {
                     Done = true;
                     Ok = (st == wgpu::MapAsyncStatus::Success);
                     if (!Ok) Log::Error("render", "map_failed", {{"msg", SvToStr(msg)}});
                   });
}

void Readback::FromTexture(const wgpu::Device &device, const wgpu::Queue &queue,
                           const wgpu::Texture &tex, wgpu::TextureAspect aspect, uint32_t width,
                           uint32_t height, uint32_t texelBytes) {
  PaddedRow = (width * texelBytes + 255u) & ~255u;
  const uint64_t bytes = (uint64_t)PaddedRow * height;

  wgpu::BufferDescriptor bd{};
  bd.size = bytes;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  Staging = device.CreateBuffer(&bd);

  wgpu::TexelCopyTextureInfo src{};
  src.texture = tex;
  src.aspect = aspect;
  wgpu::TexelCopyBufferInfo dst{};
  dst.buffer = Staging;
  dst.layout.bytesPerRow = PaddedRow;
  dst.layout.rowsPerImage = height;
  wgpu::Extent3D ext{width, height, 1};

  wgpu::CommandEncoder enc = device.CreateCommandEncoder();
  enc.CopyTextureToBuffer(&src, &dst, &ext);
  wgpu::CommandBuffer cmd = enc.Finish();
  queue.Submit(1, &cmd);
  Map(bytes);
}

void Readback::FromBuffer(const wgpu::Device &device, const wgpu::Queue &queue,
                          const wgpu::Buffer &src, uint64_t bytes) {
  PaddedRow = (uint32_t)bytes;

  wgpu::BufferDescriptor bd{};
  bd.size = bytes;
  bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  Staging = device.CreateBuffer(&bd);

  wgpu::CommandEncoder enc = device.CreateCommandEncoder();
  enc.CopyBufferToBuffer(src, 0, Staging, 0, bytes);
  wgpu::CommandBuffer cmd = enc.Finish();
  queue.Submit(1, &cmd);
  Map(bytes);
}

ReadState Readback::Poll(const wgpu::Instance &instance) {
  if (!Staging) return ReadState::Failed;
  instance.ProcessEvents();
  if (!Done) return ReadState::Pending;
  return Ok ? ReadState::Ready : ReadState::Failed;
}

const uint8_t *Readback::Rows() const {
  return static_cast<const uint8_t *>(Staging.GetConstMappedRange(0, Bytes));
}

void Readback::Release() {
  if (!Staging) return;
  if (Ok) Staging.Unmap();
  Staging = nullptr;
  Bytes = 0;
  Done = false;
  Ok = false;
}

} // namespace outshine::Render
