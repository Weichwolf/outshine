#include "ClassStructure.h"
#include "GroundClassBuffer.h"
#include "GroundStorage.h"
#include "Readback.h"
#include "Check.h"
#include "Device.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <chrono>
#include <cstring>
#include <memory>

namespace {
constexpr double kHaloM = 2.5;
constexpr uint32_t kFineCell = 0x109;
constexpr uint32_t kCoarseCell = 0x107;
constexpr uint32_t kSeedHeader = 0x203;
constexpr uint32_t kSeedSequence = 17;
constexpr uint32_t kSeedDepthBits = 0x3f000000;
constexpr std::size_t kEmptyHeaderWords = 28;
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  auto fine = std::make_shared<ClassStructure::Grid>();
  fine->W = fine->H = 1;
  fine->OrgE = -2;
  fine->OrgN = 3;
  fine->CellM = 4;
  fine->BoundaryHaloM = kHaloM;
  fine->Cells = {kFineCell, 0};
  fine->Seeds = {kSeedHeader, kSeedSequence, 1, kSeedDepthBits};
  fine->Refs = {0};
  fine->Edges = {1, 2, 3, 4};
  auto coarse = std::make_shared<ClassStructure::Grid>();
  coarse->W = coarse->H = 1;
  coarse->OrgE = coarse->OrgN = -5;
  coarse->CellM = 10;
  coarse->Cells = {kCoarseCell, 0};
  auto native = std::make_shared<const ClassStructure>(
      TangentFrame::At({}),
      fine,
      coarse,
      ClassStructure::FromRun{.Version = 13, .UnmappedRow = -1});
  CHECK(native->Evaluate(-1, 4, nullptr, nullptr) == 9 &&
            native->Evaluate(0, 0, nullptr, nullptr) == 7,
        "native CPU queries retain fine precedence and coarse coverage");
  const std::weak_ptr<const ClassStructure> sourceLifetime = native;
  const GroundClassBuffer packed(*native);
  native.reset();
  fine.reset();
  coarse.reset();
  CHECK(sourceLifetime.expired(), "the render upload does not retain the native world snapshot");
  const std::array<uint32_t, 41> expected{
      0x4u,        0x10u,       0xffffffffu, 0x0u,        0x1u,        0x1u,        0xc0000000u,
      0x40400000u, 0x40800000u, 0x1cu,       0x1eu,       0x22u,       0x23u,       0x40200000u,
      0x0u,        0x0u,        0x1u,        0x1u,        0xc0a00000u, 0xc0a00000u, 0x41200000u,
      0x27u,       0x29u,       0x29u,       0x29u,       0x0u,        0x0u,        0x0u,
      0x109u,      0x0u,        0x203u,      0x11u,       0x1u,        0x3f000000u, 0x0u,
      0x3f800000u, 0x40000000u, 0x40400000u, 0x40800000u, 0x107u,      0x0u};
  CHECK(std::ranges::equal(packed.Words(), expected),
        "shader headers, offsets, float bits and complete payload match the independent layout");
  CHECK(packed.Digest() == 0x2961d18a81c7d12eull,
        "the versioned four-word payload has a deterministic word digest");
  CHECK(packed.HeapBytes() >= sizeof(expected), "the render owner accounts its allocated words");
  const auto emptyGrid = std::make_shared<const ClassStructure::Grid>();
  const ClassStructure emptyNative(TangentFrame::At({}), emptyGrid, emptyGrid, {.UnmappedRow = 13});
  const GroundClassBuffer empty(emptyNative);
  std::array<uint32_t, kEmptyHeaderWords> emptyExpected{};
  emptyExpected[2] = 13;
  CHECK(std::ranges::equal(empty.Words(), emptyExpected),
        "empty tiers preserve the unmapped class without reading an empty edge allocation");

  auto made = Native::CreateDevice({.Validation = true});
  CHECK(made, "the actual native renderer backend is available");
  if (made) {
    const auto &device = *made;
    const std::array<float, 4> palette{0.125f, 0.25f, 0.5f, 1};
    GroundStorage storage;
    CHECK(
        storage.Replace(device.get(), packed.Words(), palette, std::chrono::seconds(5)).has_value(),
        "the independently owned render product uploads completely");
    Readback read;
    CHECK(read.FromBuffer(device.get(), storage.Classes(), sizeof(expected)) == ReadState::Ready,
          "GPU classification bytes are readable");
    if (read.Rows() != nullptr) {
      CHECK(std::memcmp(read.Rows(), expected.data(), sizeof(expected)) == 0,
            "the GPU contains the exact schema and payload after the native source died");
    }
  }
  return Report();
}
