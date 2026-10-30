#include "MaterialFieldPass.h"
#include "MaterialFieldRasterInput.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "ImpostorAtlas.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr auto kBudget = std::chrono::seconds(5);

struct Tracking final : NativeDeviceProxy {
  explicit Tracking(Device &device) : NativeDeviceProxy(device) {}

  size_t Textures = 0, MeshBuffers = 0, Submissions = 0;
  bool RefuseDepth = false;

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    ++Textures;
    if (RefuseDepth && description.Texels == Format::Depth32Float) {
      return std::unexpected(Error{.Kind = ErrorKind::OutOfMemory, .Message = "depth refused"});
    }
    return Backing().CreateTexture(description, label);
  }

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    if ((static_cast<uint32_t>(description.Use) &
         static_cast<uint32_t>(BufferUse::Vertex | BufferUse::Index)) != 0) {
      ++MeshBuffers;
    }
    return Backing().CreateBuffer(description, label);
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submissions;
    return Backing().Submit(std::move(commands));
  }
};

enum class Parts { Source, Outside, Whole, Blended, Coincident };

void Quad(Geometry &geometry,
          const char *name,
          std::array<float, 4> colour,
          float left,
          float right,
          float z,
          AlphaMode alpha = AlphaMode::Opaque) {
  Material material;
  material.BaseColour = {{colour[0], colour[1], colour[2], colour[3]}};
  material.DoubleSided = true;
  material.Unlit = true;
  material.Alpha = alpha;
  const auto row = geometry.addSurface(name, material);
  CHECK(row, "an independent reference material is defined");
  if (!row) { Failed(); }
  const auto part = geometry.addPart(name, *row);
  CHECK(part, "an independent reference quad is defined");
  if (!part) { Failed(); }
  CHECK(geometry.setPositions(
            *part,
            std::array<float, 12>{left, -.5f, z, right, -.5f, z, right, .5f, z, left, .5f, z}) &&
            geometry.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "the reference uses actual whole native triangles");
}

Geometry Model(Parts parts) {
  Geometry geometry;
  if (parts == Parts::Coincident) {
    Quad(geometry, "coincident", {0, 0, 1, 1}, -.6f, .6f, 0);
    return geometry;
  }
  if (parts != Parts::Outside) {
    Quad(geometry,
         "source",
         {1, 0, 0, 1},
         -.6f,
         .6f,
         0,
         parts == Parts::Blended ? AlphaMode::Blended : AlphaMode::Opaque);
  }
  if (parts == Parts::Outside || parts == Parts::Whole) {
    Quad(geometry, "front", {0, 0, 1, 1}, -.7f, -.1f, .6f);
    Quad(geometry, "back", {0, 0, 1, 1}, .1f, .7f, -.6f);
  }
  return geometry;
}

std::shared_ptr<const ResidentMaterialField>
Prepare(MaterialFieldPass &pass, Parts parts, int side = 32) {
  const auto input = MaterialFieldInput::Prepare(Model(parts), {.WidthPx = side, .HeightPx = side});
  CHECK(input, "the independent whole input prepares");
  if (!input) { Failed(); }
  return Require(RetryOffline([&] { return pass.Prepare(*input); }, kBudget));
}

MaterialFieldView View() {
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = 1;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Target = {.WidthPx = 32, .HeightPx = 32};
  view.Encoding = RadianceEncoding::IntegralPairs;
  return view;
}

void Settle(MaterialFieldPass &pass, Device &device) {
  if (const auto last = pass.LastSubmission()) { Require(device.Wait(*last, kBudget)); }
  Require(pass.Collect());
}

bool SamePixels(Device &device, const Texture &a, const Texture &b) {
  const auto shape = a.Description();
  CHECK(shape.Width == b.Description().Width && shape.Height == b.Description().Height &&
            shape.Texels == b.Description().Texels,
        "independent outputs share their declared raster");
  const auto first = ReadImage(device, a, {.Width = shape.Width, .Height = shape.Height});
  const auto second = ReadImage(device, b, {.Width = shape.Width, .Height = shape.Height});
  size_t texelBytes = 16;
  if (shape.Texels == Format::Depth32Float) { texelBytes = 4; }
  if (shape.Texels == Format::Rgba16Float) { texelBytes = 8; }
  for (size_t row = 0; row < shape.Height; ++row) {
    if (std::memcmp(first.Rows.data() + row * first.RowBytes,
                    second.Rows.data() + row * second.RowBytes,
                    size_t{shape.Width} * texelBytes) != 0) {
      return false;
    }
  }
  return true;
}

void JointOutputs(Tracking &device) {
  MaterialFieldPass pass(device);
  Require(pass.Configure());
  auto source = Prepare(pass, Parts::Source);
  auto outside = Prepare(pass, Parts::Outside);
  const auto whole = Prepare(pass, Parts::Whole);
  std::array additional{outside};
  const auto textures = device.Textures;
  const auto meshBuffers = device.MeshBuffers;
  const auto joint = Require(pass.Relight(*source, additional, View()));
  CHECK(device.Textures == textures + 4 && device.MeshBuffers == meshBuffers,
        "two resident sources use one raw/depth/prefix/scratch set and no new mesh buffers");
  source.reset();
  outside.reset();
  additional[0].reset();
  const auto independent = Require(pass.Relight(*whole, View()));
  CHECK(SamePixels(device, *joint.Unfiltered->Texture, *independent.Unfiltered->Texture) &&
            SamePixels(device, *joint.Depth, *independent.Depth) &&
            SamePixels(device, *joint.Filtered->Texture, *independent.Filtered->Texture),
        "joint raw colour, depth and paired prefix match independently merged whole geometry");
  Settle(pass, device);
}

void CoincidentDepthKeepsNativeDrawOrder(Tracking &device) {
  MaterialFieldPass pass(device);
  Require(pass.Configure());
  const auto first = Prepare(pass, Parts::Source);
  const auto second = Prepare(pass, Parts::Coincident);
  const std::array afterFirst{second};
  const std::array afterSecond{first};
  const auto red = Require(pass.Relight(*first, View()));
  const auto blue = Require(pass.Relight(*second, View()));
  const auto forward = Require(pass.Relight(*first, afterFirst, View()));
  const auto reverse = Require(pass.Relight(*second, afterSecond, View()));
  CHECK(SamePixels(device, *forward.Unfiltered->Texture, *red.Unfiltered->Texture) &&
            SamePixels(device, *reverse.Unfiltered->Texture, *blue.Unfiltered->Texture) &&
            !SamePixels(device, *red.Unfiltered->Texture, *blue.Unfiltered->Texture),
        "equal native D32F follows original Greater draw order, without interval admission");
  Settle(pass, device);
}

void StoredCaptureRejectsUnregisteredExtras(Tracking &device) {
  MaterialFieldPass pass(device);
  Require(pass.Configure());
  ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(size_t{16} * 16, {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1});
  std::string error;
  const auto atlas = ImpostorAtlas::Create(16, {}, 1, {Material{}}, {std::move(view)}, error);
  CHECK(atlas, "the registered stored source owns a real capture");
  if (!atlas) { Failed(); }
  const auto samples = MaterialFieldRasterInput::Prepare(*atlas, {.BytesMost = 16384});
  const auto input = MaterialFieldInput::Prepare(*atlas, 0, 16384);
  CHECK(samples && input, "the stored material and capture prepare from the same original");
  if (!samples || !input) { Failed(); }
  const auto stored = Require(RetryOffline([&] { return pass.Prepare(*input); }, kBudget));
  const auto original = Prepare(pass, Parts::Source, 16);
  const std::array additional{stored};
  auto camera = View();
  camera.Camera = samples->Capture;
  camera.Camera.EyeM[0] += .01;
  camera.Target = {.WidthPx = 16, .HeightPx = 16};
  camera.Encoding = RadianceEncoding::Texels;
  const auto textures = device.Textures;
  const auto submissions = device.Submissions;
  const auto rejected = pass.Relight(*original, additional, camera);
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid && device.Textures == textures &&
            device.Submissions == submissions,
        "a shifted additional stored capture rejects before GPU work");
  Settle(pass, device);
}

void RejectBeforeSubmission(Tracking &device) {
  MaterialFieldPass pass(device);
  MaterialFieldPass foreign(device);
  Require(pass.Configure());
  Require(foreign.Configure());
  const auto source = Prepare(pass, Parts::Source);
  const auto outside = Prepare(pass, Parts::Outside);
  const auto wrongRaster = Prepare(pass, Parts::Outside, 16);
  const auto wrongPass = Prepare(foreign, Parts::Outside);
  const auto blended = Prepare(pass, Parts::Blended);
  const auto textures = device.Textures;
  const auto submissions = device.Submissions;
  for (const auto &extra :
       {std::shared_ptr<const ResidentMaterialField>{}, wrongRaster, wrongPass, blended}) {
    const std::array additional{extra};
    const auto rejected = pass.Relight(*source, additional, View());
    CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid && device.Textures == textures &&
              device.Submissions == submissions,
          "null, raster, pass and nonopaque inputs reject before GPU allocation or submission");
  }
  const std::array additional{outside};
  const auto rejected = pass.Relight(*blended, additional, View());
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
        "a nonopaque primary cannot enter an opaque union");
  const std::atomic_bool stop{true};
  const auto cancelled = pass.Relight(*source, additional, View(), &stop);
  CHECK(!cancelled && cancelled.error().Kind == ErrorKind::Cancelled &&
            device.Textures == textures && device.Submissions == submissions,
        "a stopped union publishes no GPU work or images");
  device.RefuseDepth = true;
  const auto failed = pass.Relight(*source, additional, View());
  CHECK(!failed && failed.error().Kind == ErrorKind::OutOfMemory &&
            device.Submissions == submissions,
        "partial target failure publishes no union");
  device.RefuseDepth = false;
  Settle(pass, device);
  Settle(foreign, device);
}
}

int main() {
  if (!SDL_Init(0)) {
    CHECK(false, "SDL initializes for the native GPU test");
    return Report();
  }
  {
    auto native = Require(CreateDevice({.Validation = true}));
    Tracking device(*native);
    JointOutputs(device);
    CoincidentDepthKeepsNativeDrawOrder(device);
    StoredCaptureRejectsUnregisteredExtras(device);
    RejectBeforeSubmission(device);
  }
  SDL_Quit();
  return Report();
}
