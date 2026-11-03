#include "Check.h"
#include "GeometryRenderer.h"
#include "Gpu.h"
#include "ReadbackTransfer.h"
#include "RenderScene.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test::Gpu;

void Exercise(Native::Device &device) {
  RenderScene storage;
  GeometryRenderer draw{storage};
  outshine::Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 32;
  CHECK(gpu.SceneColours.Add(Resource::SceneLinear) && draw.Configure(gpu),
        "isolated renderer configures native mesh uploads");
  const std::array<MaterialBinding, 1> materials{};
  std::string error;
  CHECK(draw.AppendMaterials(materials, error), "pieces share one native material");
  constexpr std::array<uint32_t, 1> slots{0};
  draw.SetNativePieceSurfaces(slots);
  std::vector<float> expected;
  const auto before = draw.Resident().Allocations().DeviceCopyBytes;
  for (const uint32_t count : {3u, 3u, 3u, 7u}) {
    std::vector<StoredVertex> vertices;
    for (uint32_t vertex = 0; vertex < count; ++vertex) {
      const auto x = static_cast<float>(expected.size());
      vertices.push_back(StoredVertex::Of({{x, 2, 3}}, {{0.25f, 0.5f}}, {{0, 1, 0}}));
      expected.insert(expected.end(), {x, 2, 3});
    }
    const std::array<uint32_t, 3> indices{0, 1, 2};
    const std::vector<float> colours(size_t{count} * 4, 1);
    const std::vector<float> tangents(size_t{count} * 4, 1);
    const std::vector<float> secondUv(size_t{count} * 2, 0.5f);
    Require(draw.PlacePiece({.Tangents = tangents,
                             .Verts = vertices,
                             .Indices = indices,
                             .Clusters = {},
                             .Colours = colours,
                             .Uv1 = secondUv,
                             .Instances = {},
                             .Surface = {},
                             .Textured = true}));
  }
  const auto after = draw.Resident().Allocations();
  constexpr uint64_t vertexStride = (3 + 1 + 2 + 2 + 4 + 4) * sizeof(float);
  constexpr uint64_t expectedCopies = (3 + 6 + 9) * vertexStride + (3 + 6) * sizeof(uint32_t);
  CHECK(after.DeviceCopyBytes - before == expectedCopies && expectedCopies == 1188,
        "position, normal, UV, colour, tangent and index growth preserve only allocated prefixes");
  CHECK(after.VertexSlots == 16 && after.IndexSlots == 12 && draw.PiecesStanding() == 4,
        "every source range remains allocated and every piece remains present");
  Require(draw.Residency().SubmitPendingUploads());
  auto readback =
      Require(ReadbackTransfer::Start(device,
                                      *draw.Resident().Buffer(GeometryResidency::Stream::Vertex),
                                      {.Bytes = expected.size() * sizeof(float)}));
  const auto read = Require(readback.Wait(device, std::chrono::seconds(5)));
  CHECK(read, "all native piece positions are readable after deferred growth");
  if (read) {
    std::vector<float> actual(expected.size());
    std::memcpy(actual.data(), read->data(), read->size());
    CHECK(actual == expected, "each distinct position survives every known-prefix growth");
  }
}
}

int main() {
  auto native = Require(outshine::Render::Native::CreateDevice({.Validation = true}));
  outshine::Test::Gpu::NativeOfflineDevice device(*native);
  Exercise(device);
  return outshine::Test::Report();
}
