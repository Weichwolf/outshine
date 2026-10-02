#include "ClassStructure.h"
#include "GroundClassBuffer.h"
#include "GroundStorage.h"
#include "Readback.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  auto fine = std::make_shared<ClassStructure::Grid>();
  fine->W = fine->H = 1;
  fine->OrgE = -2;
  fine->OrgN = 3;
  fine->CellM = 4;
  fine->Cells = {0x109, 0};
  fine->Seeds = {0x10203, 17, 0x3f000000};
  fine->Refs = {0};
  fine->Edges = {1, 2, 3, 4};
  auto coarse = std::make_shared<ClassStructure::Grid>();
  coarse->W = coarse->H = 1;
  coarse->OrgE = coarse->OrgN = -5;
  coarse->CellM = 10;
  coarse->Cells = {0x107, 0};
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
  const std::array<uint32_t, 40> expected{
      4,          16, 0xffffffff, 0,          1,          1,          0xc0000000, 0x40400000,
      0x40800000, 28, 30,         33,         34,         0,          0,          0,
      1,          1,  0xc0a00000, 0xc0a00000, 0x41200000, 38,         40,         40,
      40,         0,  0,          0,          0x109,      0,          0x10203,    17,
      0x3f000000, 0,  0x3f800000, 0x40000000, 0x40400000, 0x40800000, 0x107,      0};
  CHECK(std::ranges::equal(packed.Words(), expected),
        "shader headers, offsets, float bits and complete payload match the independent layout");
  CHECK(packed.Digest() == 0x07cf19c08a298369ull,
        "the existing word digest is preserved after moving packing out of world");
  CHECK(packed.HeapBytes() >= sizeof(expected), "the render owner accounts its allocated words");
  const auto emptyGrid = std::make_shared<const ClassStructure::Grid>();
  const ClassStructure emptyNative(TangentFrame::At({}), emptyGrid, emptyGrid, {.UnmappedRow = 13});
  const GroundClassBuffer empty(emptyNative);
  std::array<uint32_t, 28> emptyExpected{};
  emptyExpected[2] = 13;
  CHECK(std::ranges::equal(empty.Words(), emptyExpected),
        "empty tiers preserve the unmapped class without reading an empty edge allocation");

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the real GPU transfer");
  SDL_GPUDevice *device = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
      false,
      nullptr);
  CHECK(device != nullptr, "the actual renderer backend is available");
  if (device) {
    {
      const std::array<float, 4> palette{0.125f, 0.25f, 0.5f, 1};
      GroundStorage storage;
      CHECK(storage.Replace(device, packed.Words(), palette).has_value(),
            "the independently owned render product uploads completely");
      Readback read;
      CHECK(read.FromBuffer(device, storage.Classes(), sizeof(expected)) == ReadState::Ready,
            "GPU classification bytes are readable");
      if (read.Rows()) {
        CHECK(std::memcmp(read.Rows(), expected.data(), sizeof(expected)) == 0,
              "the GPU contains the exact schema and payload after the native source died");
      }
    }
    SDL_DestroyGPUDevice(device);
  }
  SDL_Quit();
  return Report();
}
