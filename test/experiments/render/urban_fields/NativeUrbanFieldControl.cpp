#include <Outshine.h>
#include "TestEngine.h"
#include "NativeUrbanField.h"
#include "StoredVertex.h"
#include "AzimuthElevation.h"
#include "math/Units.h"
#include "native/Device.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <string_view>
#include <system_error>
#include <cmath>
#include <cstddef>
#include <memory>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <limits>
#include <expected>
#include <fstream>
#include <span>
#include <ratio>
#include <utility>
#include <numbers>
#include <optional>
#include <print>
#include <string>
#include <vector>

namespace UrbanControl {
namespace {
using Clock = std::chrono::steady_clock;
constexpr Extent kFrame{.WidthPx = 1280, .HeightPx = 720};
constexpr double kSunLux = 50000, kSunElevationDeg = 35, kSunBearingDeg = 210;
constexpr Vec3 kSkyRadiance{{1000, 1200, 1600}};
constexpr Vec4f kFloorColour{{0.18f, 0.2f, 0.16f, 1}};
constexpr Vec4f kBoxColour{{0.52f, 0.5f, 0.48f, 1}};
constexpr float kBoxRoughness = 0.85f;
constexpr double kOrbitScale = 0.85, kAltitudeScale = 0.4, kNearM = 0.1;

struct ViewControl {
  double RadiusM = 0;
};

enum class ChangeKind { None, Light, Boxes };

struct Sequence {
  ChangeKind Kind = ChangeKind::None;
  bool Fresh = false;
};

std::optional<Sequence> SequenceOf(std::string_view name) {
  if (name == "orbit") { return Sequence{}; }
  if (name == "relight") { return Sequence{.Kind = ChangeKind::Light}; }
  if (name == "relit") { return Sequence{.Kind = ChangeKind::Light, .Fresh = true}; }
  if (name == "replace") { return Sequence{.Kind = ChangeKind::Boxes}; }
  if (name == "replaced") { return Sequence{.Kind = ChangeKind::Boxes, .Fresh = true}; }
  return std::nullopt;
}

struct DeviceControl {
  bool Validation = false;
  size_t Creates = 0;
  Render::Native::Device *Device = nullptr;
  std::optional<Render::Native::Completion> LastSubmitted = std::nullopt;
};

struct VideoSystem {
  ~VideoSystem() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
};

Camera CameraAt(const HeightGrid &field, double angle, const ViewControl &view) {
  const double width = field.Columns * field.CellM;
  const double depth = field.Rows * field.CellM;
  const double radius = view.RadiusM;
  Camera pose;
  pose.PositionM = {{width * 0.5 + std::sin(angle) * radius,
                     radius * kAltitudeScale,
                     depth * 0.5 + std::cos(angle) * radius}};
  pose.LooksAt = true;
  pose.LookAtM = {{width * 0.5, 0, depth * 0.5}};
  pose.NearM = kNearM;
  pose.FarM = radius * 10;
  return pose;
}

outshine::Render::Native::Result<std::unique_ptr<outshine::Render::Native::Device>>
CreateDevice(void *context, outshine::Render::Native::DeviceOptions options) {
  auto &control = *static_cast<DeviceControl *>(context);
  ++control.Creates;
  options.Validation = control.Validation;
  auto device = outshine::Render::Native::CreateDevice(options);
  if (device) { control.Device = device->get(); }
  return device;
}

Render::Native::Result<Render::Native::Completion>
Submit(void *context,
       Render::Native::Device *device,
       std::unique_ptr<Render::Native::Commands> commands) {
  auto submitted = device->Submit(std::move(commands));
  if (submitted) { static_cast<DeviceControl *>(context)->LastSubmitted = *submitted; }
  return submitted;
}

struct BoxMesh {
  std::vector<StoredVertex> Vertices;
  std::vector<uint32_t> Indices;
};

BoxMesh UnitBox() {
  BoxMesh result;
  const std::array<Vec3f, 6> normals{
      {{{1, 0, 0}}, {{-1, 0, 0}}, {{0, 1, 0}}, {{0, -1, 0}}, {{0, 0, 1}}, {{0, 0, -1}}}};
  const std::array<Vec3f, 6> across{
      {{{0, 0, -1}}, {{0, 0, 1}}, {{1, 0, 0}}, {{1, 0, 0}}, {{1, 0, 0}}, {{-1, 0, 0}}}};
  const std::array<Vec3f, 6> up{
      {{{0, 1, 0}}, {{0, 1, 0}}, {{0, 0, -1}}, {{0, 0, 1}}, {{0, 1, 0}}, {{0, 1, 0}}}};
  constexpr std::array<Vec2f, 4> corners{{{{-1, -1}}, {{1, -1}}, {{1, 1}}, {{-1, 1}}}};
  for (size_t side = 0; side < normals.size(); ++side) {
    const auto first = static_cast<uint32_t>(result.Vertices.size());
    for (const auto &corner : corners) {
      Vec3f at;
      for (size_t axis = 0; axis < 3; ++axis) {
        at[axis] =
            (normals[side][axis] + across[side][axis] * corner[0] + up[side][axis] * corner[1]) *
            0.5f;
      }
      at[1] += 0.5f;
      result.Vertices.push_back(StoredVertex::Of(at, {{0, 0}}, normals[side]));
    }
    for (const uint32_t index : {0u, 1u, 2u, 0u, 2u, 3u}) {
      result.Indices.push_back(first + index);
    }
  }
  return result;
}

std::vector<Mat4> InstanceRows(const HeightGrid &field) {
  std::vector<Mat4> rows;
  rows.reserve(field.HeightsM.size());
  for (uint32_t y = 0; y < field.Rows; ++y) {
    for (uint32_t x = 0; x < field.Columns; ++x) {
      const float height = field.HeightsM[static_cast<size_t>(y) * field.Columns + x];
      if (height == 0) { continue; }
      Mat4 model;
      model.At(0, 0) = field.CellM * field.FillFraction;
      model.At(1, 1) = height;
      model.At(2, 2) = field.CellM * field.FillFraction;
      model.At(0, 3) = (static_cast<double>(x) + 0.5) * field.CellM;
      model.At(2, 3) = (static_cast<double>(y) + 0.5) * field.CellM;
      rows.push_back(model);
    }
  }
  return rows;
}

bool Accepted(const Result &result) {
  if (!result) { std::cerr << result.error() << '\n'; }
  return result.has_value();
}

bool PrepareScene(Engine &engine, const HeightGrid &field, const ViewControl &control) {
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = kFrame;
  scenario.Render.Outputs = {"sceneLinear", "sceneDepth"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = kSunLux;
  scenario.Lit.Key.ElevationDeg = kSunElevationDeg;
  scenario.Lit.Key.BearingDeg = kSunBearingDeg;
  scenario.Lit.IndirectLight = kSkyRadiance;
  const double width = field.Columns * field.CellM;
  const double depth = field.Rows * field.CellM;
  Scenario::View view;
  view.Id = "urban-field";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees = CameraAt(field, std::numbers::pi / 4, control);
  scenario.Views.push_back(view);
  Geometry floor;
  Material ground;
  ground.BaseColour = kFloorColour;
  ground.Roughness = 1;
  const auto surface = floor.addSurface("ground-control", ground);
  if (!surface) { return false; }
  const auto part = floor.addPart("ground-control", *surface);
  if (!part) { return false; }
  Geometry::Mesh mesh;
  const auto w = static_cast<float>(width);
  const auto d = static_cast<float>(depth);
  mesh.PositionsM = {0, 0, 0, w, 0, 0, w, 0, d, 0, 0, d};
  mesh.Normals = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  mesh.Indices = {0, 2, 1, 0, 3, 2};
  if (!floor.adoptMesh(*part, mesh)) { return false; }
  return Accepted(engine.setRenderTarget(kFrame)) && Accepted(engine.declare(scenario)) &&
         Accepted(engine.setGeometry(floor)) && Accepted(engine.assemble());
}

std::expected<Render::PieceHandle, std::string>
UploadBoxes(Engine &engine, const BoxMesh &mesh, std::span<const Mat4> rows) {
  auto &renderer = TestEngine::StateOf(engine).Picture.Device;
  Geometry materials;
  Material material;
  material.BaseColour = kBoxColour;
  material.Roughness = kBoxRoughness;
  if (!materials.addSurface("urban-control", material)) {
    return std::unexpected("invalid control material");
  }
  const auto slots = renderer.RegisterPieceMaterials(std::move(materials));
  if (!slots) { return std::unexpected(slots.error()); }
  Render::PieceMesh piece;
  piece.Verts = mesh.Vertices;
  piece.Indices = mesh.Indices;
  piece.Instances = rows;
  piece.MaxInstances = static_cast<uint32_t>(rows.size());
  piece.Surface = Render::PieceSurface::Registered(*slots);
  return renderer.PlacePiece(piece);
}

void DeclareShadows(Engine &engine, const HeightGrid &field, double bearingDeg = kSunBearingDeg) {
  const auto direction = EastUpSouthDirection(bearingDeg * kDeg2Rad, kSunElevationDeg * kDeg2Rad);
  const Vec3f toSun{{static_cast<float>(direction[0]),
                     static_cast<float>(direction[1]),
                     static_cast<float>(direction[2])}};
  const double radius = std::hypot(field.Columns * field.CellM, field.Rows * field.CellM) +
                        std::ranges::max(field.HeightsM);
  TestEngine::StateOf(engine).Picture.Device.SetShadowFrame(toSun, {{0, 1, 0}}, radius, false, 0);
}

bool Relight(Engine &engine, const HeightGrid &field, size_t &setterCalls) {
  constexpr double bearingDeg = 300;
  const auto direction = EastUpSouthDirection(bearingDeg * kDeg2Rad, kSunElevationDeg * kDeg2Rad);
  Render::SubjectLight light;
  light.Light.Intensity = static_cast<float>(kSunLux);
  for (size_t axis = 0; axis < 3; ++axis) {
    light.Light.Direction[axis] = -static_cast<float>(direction[axis]);
  }
  auto &renderer = TestEngine::StateOf(engine).Picture.Device;
  std::string error;
  ++setterCalls;
  if (!renderer.SetGeometryLights(std::span(&light, 1), error)) {
    std::cerr << error << '\n';
    return false;
  }
  DeclareShadows(engine, field, bearingDeg);
  return true;
}

void ChangedHeights(HeightGrid &field) {
  for (float &height : field.HeightsM) { height *= 0.5f; }
}

struct SceneChange {
  Sequence Plan;
  const BoxMesh *Mesh = nullptr;
  std::optional<Render::PieceHandle> *Piece = nullptr;
  DeviceControl *Device = nullptr;
  size_t Changes = 0;
  size_t LightSetterCalls = 0;
  bool PendingAtRelease = false;
};

bool ReplaceBoxes(Engine &engine, HeightGrid &field, SceneChange &change) {
  const auto previousFrame = change.Device->LastSubmitted;
  HeightGrid next = field;
  ChangedHeights(next);
  const auto rows = InstanceRows(next);
  std::optional<Render::PieceHandle> replacement;
  if (!rows.empty()) {
    const auto uploaded = UploadBoxes(engine, *change.Mesh, rows);
    if (!uploaded) {
      std::cerr << uploaded.error() << '\n';
      return false;
    }
    replacement = *uploaded;
  }
  auto &device = TestEngine::StateOf(engine).Picture.Device;
  if (previousFrame && *change.Piece) {
    const auto status = change.Device->Device->Poll(*previousFrame);
    if (!status) {
      std::cerr << status.error().Message << '\n';
      return false;
    }
    change.PendingAtRelease = *status == Render::Native::CompletionStatus::Pending;
  }
  if (*change.Piece) { device.ReleasePiece(**change.Piece); }
  *change.Piece = replacement;
  field = std::move(next);
  DeclareShadows(engine, field);
  return true;
}

bool UpdateScene(Engine &engine, HeightGrid &field, SceneChange &change, size_t frame) {
  constexpr size_t switchFrame = 30;
  if (!change.Plan.Fresh && change.Plan.Kind != ChangeKind::None && frame == switchFrame) {
    if (change.Plan.Kind == ChangeKind::Boxes && !ReplaceBoxes(engine, field, change)) {
      return false;
    }
    if (change.Plan.Kind == ChangeKind::Light && !Relight(engine, field, change.LightSetterCalls)) {
      return false;
    }
    ++change.Changes;
  }
  return true;
}

bool RenderOrbit(Engine &engine,
                 HeightGrid &field,
                 const ViewControl &control,
                 const std::string &output,
                 SceneChange &change) {
  constexpr size_t kFrames = 60;
  auto renderer = engine.renderer();
  if (change.Plan.Kind == ChangeKind::Light && change.Plan.Fresh &&
      !Relight(engine, field, change.LightSetterCalls)) {
    return false;
  }
  for (size_t warm = 0; warm < 3; ++warm) {
    if (!Accepted(engine.advance()) || !UpdateScene(engine, field, change, warm) ||
        !Accepted(renderer.render({}))) {
      return false;
    }
  }
  if (!Accepted(renderer.resetMeasurements(kFrames))) { return false; }
  std::vector<double> hostTimes;
  hostTimes.reserve(kFrames);
  for (size_t frame = 0; frame < kFrames; ++frame) {
    const auto began = Clock::now();
    const double theta = (1.0 + static_cast<double>(frame + 1) / kFrames) * 2 * std::numbers::pi +
                         std::numbers::pi / 4;
    const auto pose = CameraAt(field, theta, control);
    if (!Accepted(engine.setCameraPose(pose)) || !Accepted(engine.advance()) ||
        !UpdateScene(engine, field, change, frame) || !Accepted(renderer.render({}))) {
      return false;
    }
    hostTimes.push_back(std::chrono::duration<double, std::milli>(Clock::now() - began).count());
  }
  TestEngine::StateOf(engine).Picture.Device.WaitForGpu();
  std::vector<Render::GpuFrameSample> samples;
  if (!Accepted(renderer.gpuSamples(samples)) || samples.size() != kFrames) { return false; }
  std::ranges::sort(samples, {}, &Render::GpuFrameSample::Submission);
  std::ofstream times(output + ".times");
  times << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (size_t sample = 0; sample < samples.size(); ++sample) {
    times << samples[sample].Submission << ' ' << samples[sample].ElapsedMs << ' '
          << hostTimes[sample] << '\n';
  }
  if (!times.good() || !Accepted(renderer.saveScreenshot(output + ".png"))) { return false; }
  std::vector<float> depthValues;
  if (!Accepted(renderer.readPixels(Buffer::Depth, depthValues))) { return false; }
  std::ofstream depthFile(output + ".depth", std::ios::binary);
  depthFile.write(reinterpret_cast<const char *>(depthValues.data()),
                  static_cast<std::streamsize>(depthValues.size() * sizeof(float)));
  return depthFile.good();
}
}
}

int main(int argc, const char *const *argv) {
  if ((argc < 5 || argc > 8) || !SDL_Init(SDL_INIT_VIDEO)) { return 2; }
  const auto begin = UrbanControl::Clock::now();
  auto field = UrbanControl::LoadHeightGrid(
      {.Database = argv[1], .SourceSha256 = argv[2], .InputPath = argv[3]});
  if (!field) {
    std::cerr << field.error() << '\n';
    return 1;
  }
  const UrbanControl::VideoSystem video;
  UrbanControl::ViewControl control{.RadiusM = std::max(field->Columns, field->Rows) *
                                               field->CellM * UrbanControl::kOrbitScale};
  if (argc >= 6 && std::string_view(argv[5]) != "auto") {
    const std::string_view value(argv[5]);
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), control.RadiusM);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        !std::isfinite(control.RadiusM) || control.RadiusM <= 0) {
      return 2;
    }
  }
  const auto sequence = UrbanControl::SequenceOf(argc == 8 ? argv[7] : "orbit");
  if (!sequence) { return 2; }
  if (sequence->Kind == UrbanControl::ChangeKind::Boxes && sequence->Fresh) {
    UrbanControl::ChangedHeights(*field);
  }
  const auto loaded = UrbanControl::Clock::now();
  const auto rows = UrbanControl::InstanceRows(*field);
  const auto mesh = UrbanControl::UnitBox();
  if (argc >= 7 && std::string_view(argv[6]) != "validate" &&
      (argc != 8 || std::string_view(argv[6]) != "normal")) {
    return 2;
  }
  UrbanControl::DeviceControl devices{.Validation =
                                          argc >= 7 && std::string_view(argv[6]) == "validate"};
  auto engine = outshine::TestEngine::WithGpu(
      {.Context = &devices, .Create = UrbanControl::CreateDevice, .Submit = UrbanControl::Submit});
  if (!UrbanControl::PrepareScene(engine, *field, control)) { return 1; }
  std::optional<outshine::Render::PieceHandle> piece;
  if (!rows.empty()) {
    const auto uploaded = UrbanControl::UploadBoxes(engine, mesh, rows);
    if (!uploaded) {
      std::cerr << uploaded.error() << '\n';
      return 1;
    }
    piece = *uploaded;
  }
  UrbanControl::DeclareShadows(engine, *field);
  const auto prepared = UrbanControl::Clock::now();
  UrbanControl::SceneChange change{
      .Plan = *sequence, .Mesh = &mesh, .Piece = &piece, .Device = &devices};
  const bool okay = UrbanControl::RenderOrbit(engine, *field, control, argv[4], change);
  auto &device = outshine::TestEngine::StateOf(engine).Picture.Device;
  std::println(
      "URBAN instances={} prototype_vertices={} prototype_indices={} potential_references={} "
      "matrix_bytes={} load_ms={:.3f} prepare_ms={:.3f} piece_bytes={} raster_frames={} "
      "device_creations={} geometry_draw_calls={} okay={}",
      rows.size(),
      mesh.Vertices.size(),
      mesh.Indices.size(),
      rows.size() * 12,
      rows.size() * sizeof(outshine::Mat4),
      std::chrono::duration<double, std::milli>(loaded - begin).count(),
      std::chrono::duration<double, std::milli>(prepared - loaded).count(),
      device.PieceBytesHeld(),
      device.Costs().Frames,
      devices.Creates,
      device.GeometryDrawCallCount(),
      okay);
  std::println("DEVICE {}", device.GpuName());
  std::println("CHANGES applied={} pending_before_release={} light_setter_calls={}",
               change.Changes,
               change.PendingAtRelease,
               change.LightSetterCalls);
  if (piece) { device.ReleasePiece(*piece); }
  return okay ? 0 : 1;
}
