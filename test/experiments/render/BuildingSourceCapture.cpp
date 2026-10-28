#include "BuildingSourceCapture.h"
#include "BuildingMeshConversion.h"
#include "BuildingPlaneCapture.h"
#include "BuildingSurfaceBlock.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructureTile.h"
#include "ImpostorBaker.h"
#include "GpuSubmission.h"
#include "ReadTextFile.h"
#include "Geodesy.h"
#include "Box.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <ratio>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace outshine::Test {
namespace {
using namespace outshine;
using namespace outshine::Generators;
using Json = nlohmann::json;
namespace Fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr size_t kInputBytesMost = size_t{64} * 1024 * 1024;

[[noreturn]] void Fail(std::string_view reason) {
  std::println(stderr, "{}", reason);
  std::_Exit(1);
}

std::string Read(const Fs::path &path, size_t bytesMost = kInputBytesMost) {
  auto bytes = ReadTextFile(path.string(), bytesMost);
  if (!bytes) { Fail(bytes.error()); }
  return std::move(*bytes);
}

std::span<const uint8_t> Bytes(const std::string &text) {
  return {reinterpret_cast<const uint8_t *>(text.data()), text.size()};
}

double Milliseconds(Clock::time_point began) {
  return std::chrono::duration<double, std::milli>(Clock::now() - began).count();
}

Vec3 Vector(const Json &value) {
  if (!value.is_array() || value.size() != 3) { Fail("three-component vector required"); }
  const Vec3 result{
      {value.at(0).get<double>(), value.at(1).get<double>(), value.at(2).get<double>()}};
  if (!std::ranges::all_of(result, [](double x) { return std::isfinite(x); })) {
    Fail("finite vector required");
  }
  return result;
}

struct Frame {
  EnuAxes Axes;
  Vec3 Origin;

  [[nodiscard]] Vec3 Local(const Vec3 &point) const {
    const Vec3 relative = point - Origin;
    return {{Dot(relative, Axes.East), Dot(relative, Axes.Up), -Dot(relative, Axes.North)}};
  }

  [[nodiscard]] Vec3 EcefDirection(const Vec3 &value) const {
    return Axes.East * value[0] + Axes.Up * value[1] - Axes.North * value[2];
  }
};

struct SourceCosts {
  uint64_t FormReads = 0;
  uint64_t FormBytes = 0;
  size_t DescriptorBytes = 0;
  double ReadMs = 0, DecodeMs = 0;
};

void BindCell(const std::shared_ptr<PreparedStructureTile> &base,
              uint32_t cell,
              const Fs::path &path,
              SourceCosts &costs) {
  std::vector<uint32_t> indices;
  for (uint32_t at = 0; at < base->Structures.size(); ++at) {
    if (base->Structures[at].Layout.Cell.Index == cell) { indices.push_back(at); }
  }
  const auto block = std::make_shared<BuildingSurfaceBlock>(
      [root = std::weak_ptr(base), path, indices, &costs]
      -> std::expected<std::vector<BuildingSurface>, StructureMeshError> {
        const auto held = root.lock();
        if (!held) { return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable); }
        const auto readAt = Clock::now();
        const auto bytes = Read(path);
        costs.ReadMs += Milliseconds(readAt);
        ++costs.FormReads;
        costs.FormBytes += bytes.size();
        const auto decodeAt = Clock::now();
        auto models = DecodeBuildingSurfaceBlock(Bytes(bytes), kInputBytesMost);
        if (!models || models->size() != indices.size()) {
          return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
        }
        for (size_t at = 0; at < indices.size(); ++at) {
          if (!CompatiblePreparedSurface(held->Surfaces[indices[at]], (*models)[at])) {
            return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
          }
        }
        costs.DecodeMs += Milliseconds(decodeAt);
        return std::move(*models);
      });
  for (uint32_t at = 0; at < indices.size(); ++at) {
    if (base->Surfaces[indices[at]].PreparedSelection()) {
      base->Surfaces[indices[at]].BindShapes(block, at);
    }
  }
}

Geometry NativeGeometry(const Raised &raised, const Frame &frame) {
  Geometry geometry;
  Material wall;
  wall.BaseColour = {{.74f, .71f, .65f, 1}};
  wall.Roughness = .88f;
  wall.Pattern = SurfacePattern::Facade;
  Material roof;
  roof.BaseColour = {{.42f, .20f, .14f, 1}};
  roof.Roughness = .72f;
  roof.Pattern = SurfacePattern::Facade;
  Material metres = wall;
  metres.Pattern = SurfacePattern::FacadeMetres;
  for (const auto &material : std::array{wall, roof, metres}) {
    if (!geometry.addSurface("original-source", material)) { Fail("invalid source material"); }
  }
  const std::array parts{BuildingMeshPart{.Vertices = raised.WallCorners,
                                          .Indices = raised.WallRun,
                                          .Colours = raised.WallColours,
                                          .MaterialM = raised.WallMaterialM},
                         BuildingMeshPart{.Vertices = raised.RoofCorners,
                                          .Indices = raised.RoofRun,
                                          .Colours = {},
                                          .MaterialM = {}}};
  for (size_t at = 0; at < parts.size(); ++at) {
    if (parts[at].Vertices.empty()) { continue; }
    const int material = at == 0 && !parts[at].MaterialM.empty() ? 2 : static_cast<int>(at);
    auto mesh = ConvertBuildingMesh(parts[at], frame.Axes, frame.Origin);
    if (!mesh) { Fail(mesh.error()); }
    const auto part = geometry.addPart("source-plane", MaterialInstance(material));
    if (!part || !geometry.adoptMesh(*part, *mesh)) { Fail("invalid source plane geometry"); }
  }
  return geometry;
}

template <class T> void Write(const Fs::path &path, std::span<const T> bytes) {
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size_bytes()));
  if (!stream) { Fail("output write failed"); }
}

void SaveGeometry(const Geometry &geometry, const Fs::path &path) {
  Fs::create_directories(path);
  Json parts = Json::array();
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto directory = path / std::to_string(part);
    Fs::create_directories(directory);
    const size_t vertices = geometry.positionsOf(part).size() / 3;
    Write<float>(directory / "positions.bin", geometry.positionsOf(part));
    Write<float>(directory / "normals.bin", geometry.normalsOf(part));
    Write<float>(directory / "uv.bin", geometry.textureOf(part));
    Write<uint32_t>(directory / "indices.bin", geometry.trianglesOf(part));
    const auto uv1 = geometry.textureOf(part, UvSet::Uv1);
    const auto colours = geometry.coloursOf(part);
    const std::vector<float> defaultUv(uv1.empty() ? vertices * 2 : 0);
    const std::vector<float> defaultColour(colours.empty() ? vertices * 4 : 0, 1.0f);
    Write<float>(directory / "uv1.bin", uv1.empty() ? std::span<const float>(defaultUv) : uv1);
    Write<float>(directory / "colours.bin",
                 colours.empty() ? std::span<const float>(defaultColour) : colours);
    parts.push_back({{"vertices", vertices},
                     {"triangles", geometry.trianglesOf(part).size() / 3},
                     {"material", geometry.materialOf(part).index()}});
  }
  std::ofstream description(path / "input.json");
  description << Json{{"parts", parts}}.dump(2) << '\n';
  if (!description) { Fail("source mesh description write failed"); }
}

ImpostorAtlas CapturePlaneViews(const BuildingPlaneCapture &producer,
                                const Frame &frame,
                                const Render::ImpostorCapture &metadata,
                                const Render::ImpostorCaptureProjection &projection,
                                int pixels,
                                uint64_t sourceTests,
                                Json &report,
                                Render::ImpostorBaker &baker,
                                const Fs::path *geometryOutput) {
  std::string error;
  std::vector<ImpostorAtlas::View> views;
  std::vector<Material> materials;
  for (const auto &direction : metadata.ViewDirections) {
    const auto screen = ImpostorViewFrame::From(direction);
    if (!screen) { Fail("invalid source direction"); }
    const auto started = Clock::now();
    auto sampled =
        producer.Capture({.Centre = frame.Origin + frame.EcefDirection(projection.Centre),
                          .TowardEye = frame.EcefDirection(direction),
                          .HalfExtentM = projection.HalfExtentM,
                          .Pixels = static_cast<uint32_t>(pixels),
                          .Frame = ImpostorViewFrame{.Right = frame.EcefDirection(screen->Right),
                                                     .Up = frame.EcefDirection(screen->Up)}},
                         {.Samples = static_cast<uint64_t>(pixels) * static_cast<uint64_t>(pixels),
                          .SourceTests = sourceTests});
    if (!sampled) { Fail("analytical source-plane capture failed"); }
    const auto sourceDone = Clock::now();
    Render::ImpostorCapture capture;
    capture.LeastM = metadata.LeastM;
    capture.MostM = metadata.MostM;
    capture.HalfExtentM = projection.HalfExtentM;
    capture.ViewDirections = {direction};
    auto geometry = NativeGeometry(sampled->Mesh, frame);
    const auto conversionDone = Clock::now();
    if (geometryOutput != nullptr) {
      SaveGeometry(geometry, *geometryOutput / "geometry" / std::to_string(views.size()));
    }
    const auto captureStarted = Clock::now();
    capture.Sources.push_back({.Mesh = std::move(geometry), .Instances = {Mat4{}}});
    auto atlas = baker.Capture(std::move(capture), {.Pixels = pixels, .Views = 1}, error);
    if (!atlas) { Fail(error); }
    if (materials.empty()) { materials = atlas->Surfaces(); }
    if (materials != atlas->Surfaces()) { Fail("source material tables differ between views"); }
    views.push_back(atlas->Views().front());
    report["views"].push_back(
        {{"rays", sampled->Rays},
         {"covered", sampled->Covered},
         {"source_tests", sampled->SourceTests},
         {"triangles", (sampled->Mesh.WallRun.size() + sampled->Mesh.RoofRun.size()) / 3},
         {"vertices", sampled->Mesh.WallCorners.size() + sampled->Mesh.RoofCorners.size()},
         {"source_ms", std::chrono::duration<double, std::milli>(sourceDone - started).count()},
         {"conversion_ms",
          std::chrono::duration<double, std::milli>(conversionDone - sourceDone).count()},
         {"export_ms",
          std::chrono::duration<double, std::milli>(captureStarted - conversionDone).count()},
         {"capture_ms",
          std::chrono::duration<double, std::milli>(Clock::now() - captureStarted).count()}});
  }
  auto atlas = ImpostorAtlas::Create(pixels,
                                     projection.Centre,
                                     projection.HalfExtentM,
                                     std::move(materials),
                                     std::move(views),
                                     error);
  if (!atlas) { Fail(error); }
  return std::move(*atlas);
}

}

struct BuildingSourceCapture::State {
  std::shared_ptr<PreparedStructureTile> Base;
  Frame LocalFrame;
  std::optional<BuildingPlaneCapture> Producer;
  Render::ImpostorCapture Metadata;
  Render::ImpostorCaptureProjection Projection;
  int Pixels = 0;
  uint64_t SourceTests = 0;
  SourceCosts Costs;
  Json Report;
  size_t Devices = 0;
  std::unique_ptr<Render::ImpostorBaker> Baker;

  State(const Fs::path &inputPath, const Json &request) {
    auto &costs = Costs;
    const auto descriptorReadAt = Clock::now();
    const auto descriptor = Read(inputPath, size_t{1024} * 1024);
    costs.ReadMs += Milliseconds(descriptorReadAt);
    costs.DescriptorBytes = descriptor.size();
    const auto descriptorDecodeAt = Clock::now();
    const auto input = Json::parse(descriptor, nullptr, false);
    costs.DecodeMs += Milliseconds(descriptorDecodeAt);
    if (!input.is_object()) { Fail("object metadata required"); }
    if (request.contains("parent") && request.at("parent") != input.at("parent")) {
      Fail("request/source parent identity mismatch");
    }
    const auto indexReadAt = Clock::now();
    const auto bytes = Read(inputPath.parent_path() / input.at("index").get<std::string>());
    costs.ReadMs += Milliseconds(indexReadAt);
    const auto indexDecodeAt = Clock::now();
    auto decoded = DecodePreparedStructureIndex(Bytes(bytes), kInputBytesMost);
    costs.DecodeMs += Milliseconds(indexDecodeAt);
    if (!decoded || decoded->Structures.size() != decoded->Surfaces.size()) {
      Fail("invalid prepared index");
    }
    const auto base = std::make_shared<PreparedStructureTile>(std::move(*decoded));
    const Frame frame{.Axes = {.East = Vector(request.at("east")),
                               .North = Vector(request.at("north")),
                               .Up = Vector(request.at("up"))},
                      .Origin = Vector(request.at("origin"))};
    const auto cells = request.at("cells").get<std::unordered_set<uint32_t>>();
    for (const auto &form : input.at("forms")) {
      const uint32_t cell = form.at("cell");
      if (cells.contains(cell)) {
        BindCell(base, cell, inputPath.parent_path() / form.at("file").get<std::string>(), costs);
      }
    }
    std::vector<const BuildingSurface *> sources;
    std::vector<uint32_t> ordinals;
    Json remainder = Json::array();
    Box bounds;
    for (uint32_t at = 0; at < base->Surfaces.size(); ++at) {
      if (!cells.contains(base->Structures[at].Layout.Cell.Index)) { continue; }
      const auto &source = base->Surfaces[at];
      if (!source.SupportsProjection() || source.FaceCount() == 0 || source.Bounds().Empty()) {
        remainder.push_back({{"source", at}, {"reason", "unsupported analytical source metadata"}});
        continue;
      }
      sources.push_back(&source);
      ordinals.push_back(at);
      for (unsigned corner = 0; corner < 8; ++corner) {
        bounds.Cover(frame.Local(source.Bounds().Corner(corner)));
      }
    }
    auto producer = BuildingPlaneCapture::Prepare(sources);
    if (!producer) { Fail("source hierarchy could not prepare"); }
    const int pixels = request.at("pixels").get<int>();
    Render::ImpostorCapture metadata;
    metadata.LeastM = bounds.Min;
    metadata.MostM = bounds.Max;
    for (const auto &direction : request.at("directions")) {
      metadata.ViewDirections.push_back(Vector(direction));
    }
    const auto count = static_cast<unsigned>(metadata.ViewDirections.size());
    std::string error;
    const auto projection =
        Render::ImpostorBaker::ProjectionFor(metadata, {.Pixels = pixels, .Views = count}, error);
    if (!projection || costs.FormReads != 0) {
      Fail("projection must be valid before source-form IO");
    }
    Json report{{"parent", input.at("parent")},
                {"index_bytes", bytes.size()},
                {"sources", ordinals},
                {"remainder", remainder},
                {"views", Json::array()},
                {"least", {bounds.Min[0], bounds.Min[1], bounds.Min[2]}},
                {"most", {bounds.Max[0], bounds.Max[1], bounds.Max[2]}},
                {"half_extent_m", projection->HalfExtentM}};
    Base = base;
    LocalFrame = frame;
    Producer = std::move(*producer);
    Metadata = std::move(metadata);
    Projection = *projection;
    Pixels = pixels;
    SourceTests = request.at("source_tests").get<uint64_t>();
    Report = std::move(report);
  }
};

BuildingSourceCapture::BuildingSourceCapture(const Fs::path &input, const Json &request)
    : State_(std::make_unique<State>(input, request)) {}

BuildingSourceCapture::~BuildingSourceCapture() = default;

const Render::ImpostorCapture &BuildingSourceCapture::Metadata() const noexcept {
  return State_->Metadata;
}

const Render::ImpostorCaptureProjection &BuildingSourceCapture::Projection() const noexcept {
  return State_->Projection;
}

const Json &BuildingSourceCapture::Report() const {
  State_->Report.update({{"form_reads", State_->Costs.FormReads},
                         {"form_bytes", State_->Costs.FormBytes},
                         {"descriptor_bytes", State_->Costs.DescriptorBytes},
                         {"source_read_ms", State_->Costs.ReadMs},
                         {"source_decode_ms", State_->Costs.DecodeMs},
                         {"device_creations", State_->Devices}});
  return State_->Report;
}

ImpostorAtlas BuildingSourceCapture::Capture(std::optional<size_t> view,
                                             const Fs::path &geometryOutput) {
  auto &state = *State_;
  Render::ImpostorCapture metadata{.Sources = {},
                                   .LeastM = state.Metadata.LeastM,
                                   .MostM = state.Metadata.MostM,
                                   .ViewDirections = state.Metadata.ViewDirections,
                                   .HalfExtentM = state.Metadata.HalfExtentM};
  if (view) {
    if (*view >= metadata.ViewDirections.size()) { Fail("source view outside catalogue"); }
    state.Report["selected_view"] = *view;
    metadata.ViewDirections = {metadata.ViewDirections[*view]};
  } else {
    state.Report.erase("selected_view");
  }
  if (!state.Baker) {
    const Render::GpuSubmission submission{
        .Context = &state.Devices,
        .Create = [](void *context, Render::Native::DeviceOptions options)
            -> Render::Native::Result<std::unique_ptr<Render::Native::Device>> {
          ++*static_cast<size_t *>(context);
          return Render::Native::CreateDevice(options);
        }};
    state.Baker = std::make_unique<Render::ImpostorBaker>(submission);
  }
  state.Report["views"] = Json::array();
  if (!state.Producer) { Fail("source hierarchy is unavailable"); }
  return CapturePlaneViews(*state.Producer,
                           state.LocalFrame,
                           metadata,
                           state.Projection,
                           state.Pixels,
                           state.SourceTests,
                           state.Report,
                           *state.Baker,
                           geometryOutput.empty() ? nullptr : &geometryOutput);
}
}
