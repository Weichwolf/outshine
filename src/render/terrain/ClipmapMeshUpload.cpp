#include "ClipmapRenderer.h"
#include "ClipmapTopology.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
enum class MeshPhase : uint32_t { Centre, SouthWest, SouthEast, NorthWest, NorthEast };

ClipmapShape Shape(uint32_t half, MeshPhase phase) {
  const auto index = static_cast<uint32_t>(phase);
  ClipmapShape shape{.HalfCells = half};
  if (index != 0) { shape.HolePhase = {{(index - 1u) % 2u, (index - 1u) / 2u}}; }
  return shape;
}

Native::Result<uint64_t> MeshBytes(ClipmapShape request, size_t geometryBytesMost) {
  uint64_t bytes = 0;
  size_t scratch = 0;
  size_t metadata = 0;
  for (uint32_t i = 0; i < 5; ++i) {
    const auto size = ClipmapTopology::Measure(Shape(request.HalfCells, static_cast<MeshPhase>(i)));
    if (!size || size->IndexBytes / sizeof(uint32_t) > std::numeric_limits<uint32_t>::max()) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "invalid native clipmap topology"});
    }
    bytes += size->VertexBytes + size->IndexBytes;
    metadata += size->BlockBytes;
    scratch = std::max(scratch, size->ScratchBytes);
  }
  if (geometryBytesMost < scratch || metadata > (geometryBytesMost - scratch) / 2u ||
      bytes > (geometryBytesMost - scratch - 2u * metadata) / 3u) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Busy,
                      .Message = "clipmap geometry exceeds CPU/GPU/staging admission"});
  }
  return bytes;
}
}

Native::Result<void>
ClipmapRenderer::BuildMeshes(Native::Device &device, uint32_t halfCells, size_t geometryBytesMost) {
  const auto bytes = MeshBytes({.HalfCells = halfCells}, geometryBytesMost);
  if (!bytes) { return std::unexpected(bytes.error()); }
  std::array<Mesh, 5> meshes;
  auto staging = device.CreateBuffer(
      {.Bytes = *bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = device.MapWrite(**staging, {.Bytes = *bytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  uint64_t offset = 0;
  for (uint32_t i = 0; i < meshes.size(); ++i) {
    const auto topology =
        ClipmapTopology::Make(Shape(halfCells, static_cast<MeshPhase>(i)), geometryBytesMost);
    if (!topology) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "clipmap mesh construction failed"});
    }
    const auto vertices = std::as_bytes(std::span(topology->Vertices));
    const auto indices = std::as_bytes(std::span(topology->Indices));
    auto vertex = device.CreateBuffer(
        {.Bytes = vertices.size(),
         .Use = Native::BufferUse::Vertex | Native::BufferUse::CopyDestination});
    auto index =
        device.CreateBuffer({.Bytes = indices.size(),
                             .Use = Native::BufferUse::Index | Native::BufferUse::CopyDestination});
    if (!vertex) { return std::unexpected(vertex.error()); }
    if (!index) { return std::unexpected(index.error()); }
    std::memcpy(mapped->data() + offset, vertices.data(), vertices.size());
    offset += vertices.size();
    std::memcpy(mapped->data() + offset, indices.data(), indices.size());
    offset += indices.size();
    meshes[i] = {.Vertices = std::move(*vertex),
                 .Indices = std::move(*index),
                 .Count = static_cast<uint32_t>(topology->Indices.size())};
  }
  const auto unmapped = device.Unmap(**staging);
  if (!unmapped) { return unmapped; }
  auto commands = device.Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  offset = 0;
  for (const auto &mesh : meshes) {
    for (auto *buffer : {mesh.Vertices.get(), mesh.Indices.get()}) {
      const auto copied = (*commands)->Copy(
          **staging, *buffer, {.SourceOffset = offset, .Bytes = buffer->Description().Bytes});
      if (!copied) { return copied; }
      offset += buffer->Description().Bytes;
    }
  }
  const auto submitted = device.Submit(std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  Meshes_ = std::move(meshes);
  HalfCells_ = halfCells;
  return {};
}

uint64_t ClipmapRenderer::HeldBytes() const noexcept {
  uint64_t bytes = 0;
  for (const auto &mesh : Meshes_) {
    for (const auto *buffer : {mesh.Vertices.get(), mesh.Indices.get()}) {
      if (buffer != nullptr) { bytes += buffer->Description().Bytes; }
    }
  }
  return bytes;
}
}
