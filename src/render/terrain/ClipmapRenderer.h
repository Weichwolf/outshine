#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPRENDERER_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPRENDERER_H
#include "ClipmapRing.h"
#include "ClipmapTopology.h"
#include "Mat4.h"
#include "device/native/Device.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Render {
struct PassRecording;
struct SurfaceOutputs;

struct ClipmapFields {
  const Native::Texture *Heights = nullptr;
  const Native::Texture *Normals = nullptr;
  const Native::Buffer *RingBuffer = nullptr;
  std::span<const ClipmapRing> Rings;
  std::span<const std::array<float, 2>> HeightRanges;
  float HeightShift = 0;
};

enum class ClipmapPass { Surface, Depth };

struct ClipmapDrawRange {
  uint32_t Level = 0;
  uint32_t First = 0;
  uint32_t Count = 0;
};

struct ClipmapDrawList {
  std::vector<ClipmapDrawRange> Ranges;
  uint32_t Triangles = 0;
};

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
                                            ClipmapPass pass = ClipmapPass::Surface,
                                            const ClipmapDrawList *selection = nullptr) const;
  [[nodiscard]] Native::Result<void>
  Select(const ClipmapFields &fields, const Mat4f &clipFromRelative, ClipmapDrawList &into) const;
  [[nodiscard]] uint64_t HeldBytes() const noexcept;

private:
  struct Mesh {
    std::unique_ptr<Native::Buffer> Vertices;
    std::unique_ptr<Native::Buffer> Indices;
    uint32_t Count = 0;
    std::vector<ClipmapBlock> Blocks;
  };

  [[nodiscard]] Native::Result<void>
  BuildMeshes(Native::Device &device, uint32_t halfCells, size_t geometryBytesMost);
  [[nodiscard]] Native::Result<void> Validate(const ClipmapFields &fields, ClipmapPass pass) const;
  [[nodiscard]] static size_t MeshIndex(const ClipmapFields &fields, size_t level) noexcept;
  std::array<Mesh, 5> Meshes_;
  std::unique_ptr<Native::GraphicsPipeline> Surface_, Depth_;
  std::unique_ptr<Native::Sampler> Nearest_, Linear_;
  uint32_t HalfCells_ = 0;
};
}
#endif
