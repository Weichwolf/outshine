#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPRENDERER_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPRENDERER_H
#include "ClipmapRing.h"
#include "device/native/Device.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace outshine::Render {
struct PassRecording;
struct SurfaceOutputs;

struct ClipmapFields {
  const Native::Texture *Heights = nullptr;
  const Native::Texture *Normals = nullptr;
  const Native::Buffer *RingBuffer = nullptr;
  std::span<const ClipmapRing> Rings;
};

enum class ClipmapPass { Surface, Depth };

class ClipmapRenderer {
public:
  [[nodiscard]] Native::Result<void> Configure(Native::Device &device,
                                               const SurfaceOutputs &outputs,
                                               std::span<const Native::ColourTarget> targets,
                                               uint32_t halfCells,
                                               size_t geometryBytesMost);
  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into,
                                            const ClipmapFields &fields,
                                            std::span<const Native::Binding> bindings,
                                            ClipmapPass pass = ClipmapPass::Surface) const;
  [[nodiscard]] uint64_t HeldBytes() const noexcept;

private:
  struct Mesh {
    std::unique_ptr<Native::Buffer> Vertices;
    std::unique_ptr<Native::Buffer> Indices;
    uint32_t Count = 0;
  };

  [[nodiscard]] Native::Result<void>
  BuildMeshes(Native::Device &device, uint32_t halfCells, size_t geometryBytesMost);
  [[nodiscard]] Native::Result<void> Validate(const ClipmapFields &fields, ClipmapPass pass) const;
  std::array<Mesh, 5> Meshes_;
  std::unique_ptr<Native::GraphicsPipeline> Surface_, Depth_;
  std::unique_ptr<Native::Sampler> Nearest_, Linear_;
  uint32_t HalfCells_ = 0;
};
}
#endif
