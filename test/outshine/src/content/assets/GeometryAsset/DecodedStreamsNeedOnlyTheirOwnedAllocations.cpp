#include "content/GeometryAsset.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <cstdlib>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace {
constexpr size_t kLargeBytes = size_t{512} * 1024;
bool Tracking = false;
size_t Allocations = 0;
size_t AllocatedBytes = 0;
}

void *operator new(std::size_t bytes) {
  void *memory = std::malloc(bytes == 0 ? 1 : bytes);
  if (memory == nullptr) { std::abort(); }
  if (Tracking && bytes >= kLargeBytes) {
    ++Allocations;
    AllocatedBytes += bytes;
  }
  return memory;
}

void operator delete(void *memory) noexcept {
  std::free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
  std::free(memory);
}

namespace {
using namespace outshine;
using namespace outshine::Test;

std::vector<uint8_t> Product() {
  constexpr size_t kVertices = 131072;
  constexpr size_t kLimit = size_t{32} * 1024 * 1024;
  Geometry geometry;
  const auto part = geometry.addPart("streams", MaterialInstance{});
  CHECK(part.has_value(), "the fixture creates a native part");
  if (!part) { return {}; }
  std::vector<float> values(kVertices * 3);
  CHECK(geometry.setPositions(*part, values), "the fixture stores its position stream");
  for (size_t at = 2; at < values.size(); at += 3) { values[at] = 1; }
  CHECK(geometry.setNormals(*part, values), "the fixture stores unit normals");
  values.assign(kVertices * 2, 0);
  CHECK(geometry.setTexture(*part, values) && geometry.setTexture(*part, values, 1),
        "both UV streams are present");
  values.assign(kVertices * 4, 1);
  CHECK(geometry.setTangents(*part, values) && geometry.setColours(*part, values),
        "tangent and colour streams are present");
  CHECK(geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the fixture has a complete indexed triangle");
  auto encoded = EncodeGeometryAsset(geometry, kLimit);
  CHECK(encoded.has_value(), "the source encodes as a complete native asset");
  return encoded ? std::move(*encoded) : std::vector<uint8_t>{};
}
}

int main() {
  const auto bytes = Product();
  Tracking = true;
  auto decoded = outshine::DecodeGeometryAsset(bytes, bytes.size());
  Tracking = false;
  CHECK(decoded.has_value(), "the complete product decodes");
  if (!decoded) { return outshine::Test::Report(); }
  const std::array streams{decoded->positionsOf(0),
                           decoded->normalsOf(0),
                           decoded->textureOf(0),
                           decoded->textureOf(0, outshine::UvSet::Uv1),
                           decoded->tangentsOf(0),
                           decoded->coloursOf(0)};
  size_t expectedBytes = 0;
  for (const auto stream : streams) { expectedBytes += stream.size_bytes(); }
  std::println("decode large allocations={} bytes={} owned_stream_bytes={}",
               Allocations,
               AllocatedBytes,
               expectedBytes);
  CHECK(Allocations == streams.size() && AllocatedBytes == expectedBytes,
        "decoding allocates each owned stream once without large scratch buffers");
  const auto replay = outshine::EncodeGeometryAsset(*decoded, bytes.size());
  CHECK(replay && *replay == bytes, "ownership changes preserve every encoded byte");
  return outshine::Test::Report();
}
