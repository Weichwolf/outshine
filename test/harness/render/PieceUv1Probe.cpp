#include "SubjectDraw.h"
#include "SceneResources.h"
#include "Check.h"
#include "Gpu.h"
#include "ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "NativeOfflineDevice.h"
#include "VertexArms.h"
#include "DrawList.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <limits>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr std::array<float, 6> kMetres{0, 0, 3, 0, 3, 2};
constexpr float kChangedInput = 99.0f;

void CheckResident(Native::Device &device, SubjectDraw &subjects) {
  std::string error;
  CHECK(subjects.HandTables(error), error.c_str());
  CHECK(std::ranges::any_of(subjects.Drawn(),
                            [](const DrawBatch &batch) { return CarriesUv1(batch.Layout); }),
        "the actual piece selects UV1 without a UV1 material image");
  Require(subjects.Owned().SubmitPendingUploads());
  const auto &buffer = subjects.Resident().Buffer(SubjectResidency::Stream::Uv1);
  CHECK(buffer != nullptr, "actual optional UV1 residency has its own buffer");
  if (!buffer) { return; }
  auto read = Require(ReadbackTransfer::Start(device, *buffer, {.Bytes = sizeof(kMetres)}));
  const auto data = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(data && data->size() == sizeof(kMetres), "actual optional UV1 residency is readable");
  if (data && data->size() == sizeof(kMetres)) {
    std::array<float, kMetres.size()> observed{};
    std::memcpy(observed.data(), data->data(), sizeof(observed));
    CHECK(observed == kMetres, "the GPU contains the original independent float2 data");
  }
  const auto allocations = subjects.Resident().Allocations();
  std::println(R"({{"uv1_buffer_bytes":{},"vertex_slots":{},"transfer_bytes":{}}})",
               allocations.StreamBytes[static_cast<size_t>(SubjectResidency::Stream::Uv1)],
               allocations.VertexSlots,
               allocations.TransferBytes);
}

void GpuOwnership(Native::Device &device) {
  outshine::Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  constexpr int kExtent = 32;
  gpu.Width = kExtent;
  gpu.Height = kExtent;
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "the actual subject pipeline has an HDR target");
  SubjectPipelineBinding binding;
  SubjectDraw subjects;
  std::string error;
  Require(subjects.Configure(binding, gpu, nullptr, nullptr, false));
  Require(subjects.AttachPipelines(binding, gpu));
  SceneResources kept;
  {
    SceneResources resources;
    Geometry materials;
    CHECK(materials.addSurface("plain", Material{}), "the fixture registers a real plain material");
    Material metres;
    metres.Pattern = SurfacePattern::FacadeMetres;
    CHECK(materials.addSurface("metres", metres),
          "the explicit metre material registers without images");
    const auto first = resources.RegisterPieceMaterials(subjects, nullptr, std::move(materials));
    CHECK(first.has_value(), "actual piece material registration succeeds");
    if (!first) { return; }
    const std::array<StoredVertex, 3> vertices{};
    const std::array<uint32_t, 3> indices{0, 1, 2};
    auto coordinates = kMetres;
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Uv1 = coordinates;
    piece.Textured = true;
    piece.Surface = PieceSurface::Registered(*first + 1u);
    const auto placed = resources.PlacePiece(subjects, piece);
    CHECK(placed.has_value(), "the real piece path admits finite UV1 data");
    if (!placed) { return; }
    const size_t withUv1 = resources.PieceSourceBytes();
    PieceMesh missing = piece;
    missing.Uv1 = {};
    CHECK(!resources.PlacePiece(subjects, missing),
          "an explicit metre material rejects missing UV1 instead of sampling zeros");
    CHECK(resources.PieceSourceCount() == 1 && resources.PieceSourceBytes() == withUv1,
          "failed metre admission preserves the original owned piece only");
    PieceMesh legacy = piece;
    legacy.Uv1 = {};
    legacy.Surface = PieceSurface::Registered(*first);
    const auto plain = resources.PlacePiece(subjects, legacy);
    CHECK(plain.has_value(), "a piece without optional data uses the existing path");
    if (!plain) { return; }
    const size_t withoutUv1 = resources.PieceSourceBytes() - withUv1;
    CHECK(withUv1 - withoutUv1 == sizeof(kMetres),
          "the owned source copies exactly the additional float2 bytes");
    std::println(
        R"({{"source_with_uv1_bytes":{},"source_without_uv1_bytes":{}}})", withUv1, withoutUv1);
    coordinates.fill(kChangedInput);
    CheckResident(device, subjects);
    CHECK(std::ranges::any_of(subjects.Drawn(),
                              [](const DrawBatch &batch) { return !CarriesUv1(batch.Layout); }),
          "a legacy piece retains its original layout beside the UV1 piece");
    kept.CopySourcesFrom(resources, SceneResources::PieceSources::Copy);
  }
  SubjectDraw restored;
  Require(restored.AttachPipelines(binding, gpu));
  CHECK(kept.RestorePieceMaterials(restored, nullptr, error), error.c_str());
  CHECK(kept.RestorePieces(restored, error), error.c_str());
  CheckResident(device, restored);
  CHECK(kept.PieceSourceCount() == 2,
        "UV1 remains owned after original inputs and source scene die");
}
}

int main(int argc, char *const *argv) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  const std::array<StoredVertex, 3> vertices{};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  std::array<float, 6> coordinates{0, 0, 3, 0, 3, 2};
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  CHECK(ValidPieceUv1(piece),
        "an absent optional UV1 stream preserves the existing piece contract");
  piece.Textured = true;
  piece.Uv1 = coordinates;
  CHECK(ValidPieceUv1(piece),
        "physical metre values outside normalized texture range remain valid");
  SubjectDraw subjects;
  SceneResources resources;
  std::string error;
  const auto reject = [&] {
    CHECK(!ValidPieceUv1(piece), "inconsistent second UV input is invalid");
    CHECK(subjects.PlacePiece(piece, error) == kNoPiece && error == kPieceUv1Error,
          "the actual subject entry rejects malformed data before device access");
    const auto result = resources.PlacePiece(subjects, piece);
    CHECK(!result && result.error() == kPieceUv1Error,
          "the scene resource entry rejects malformed data before taking an ownership copy");
    CHECK(resources.PieceSourceCount() == 0 && subjects.PiecesStanding() == 0,
          "failed UV1 admission leaves no owned source or resident piece");
  };
  piece.Uv1 = std::span(coordinates).first(5);
  reject();
  piece.Uv1 = coordinates;
  piece.Textured = false;
  reject();
  piece.Textured = true;
  coordinates[0] = std::numeric_limits<float>::infinity();
  reject();
  coordinates[0] = std::numeric_limits<float>::quiet_NaN();
  reject();
  coordinates[0] = 0;
  CHECK(ValidPieceUv1(piece), "the complete finite source is admissible again");
  if (argc == 2 && std::string_view(argv[1]) == "--gpu") {
    CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initializes for the actual existing residency path");
    {
      auto actual = Require(
          Native::CreateDevice({.Validation = SDL_getenv("OUTSHINE_GPU_VALIDATION") != nullptr}));
      NativeOfflineDevice device(*actual);
      GpuOwnership(device);
    }
    SDL_Quit();
  }
  return Report();
}
