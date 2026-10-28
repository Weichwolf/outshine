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

Json Document(const Fs::path &path) {
  const auto data = Json::parse(Read(path, size_t{1024} * 1024), nullptr, false);
  if (!data.is_object()) { Fail("object metadata required"); }
  return data;
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
        const auto bytes = Read(path);
        ++costs.FormReads;
        costs.FormBytes += bytes.size();
        auto models = DecodeBuildingSurfaceBlock(Bytes(bytes), kInputBytesMost);
        if (!models || models->size() != indices.size()) {
          return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
        }
        for (size_t at = 0; at < indices.size(); ++at) {
          if (!CompatiblePreparedSurface(held->Surfaces[indices[at]], (*models)[at])) {
            return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
          }
        }
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

void SaveFields(const ImpostorAtlas &atlas, const Fs::path &path) {
  std::vector<float> base;
  for (const auto &view : atlas.Views()) {
    for (size_t at = 0; at < view.Texels.size(); ++at) {
      for (const auto channel : view.Materials[at].BaseColour) { base.push_back(channel); }
      base.push_back(view.Texels[at].Surface == 0 ? 0.0f : 1.0f);
    }
  }
  Write<float>(path / "base.linear", base);
}

ImpostorAtlas CapturePlaneViews(const BuildingPlaneCapture &producer,
                                const Frame &frame,
                                const Render::ImpostorCapture &metadata,
                                const Render::ImpostorCaptureProjection &projection,
                                int pixels,
                                uint64_t sourceTests,
                                Json &report) {
  std::string error;
  size_t devices = 0;
  const Render::GpuSubmission submission{
      .Context = &devices,
      .Create = [](void *context, Render::Native::DeviceOptions options)
          -> Render::Native::Result<std::unique_ptr<Render::Native::Device>> {
        ++*static_cast<size_t *>(context);
        return Render::Native::CreateDevice(options);
      }};
  Render::ImpostorBaker baker(submission);
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
    capture.Sources.push_back(
        {.Mesh = NativeGeometry(sampled->Mesh, frame), .Instances = {Mat4{}}});
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
         {"capture_ms",
          std::chrono::duration<double, std::milli>(Clock::now() - sourceDone).count()}});
  }
  report["device_creations"] = devices;
  auto atlas = ImpostorAtlas::Create(pixels,
                                     projection.Centre,
                                     projection.HalfExtentM,
                                     std::move(materials),
                                     std::move(views),
                                     error);
  if (!atlas) { Fail(error); }
  return std::move(*atlas);
}

void Run(const Fs::path &inputPath, const Json &request, const Fs::path &output) {
  const auto began = Clock::now();
  const auto input = Document(inputPath);
  const auto bytes = Read(inputPath.parent_path() / input.at("index").get<std::string>());
  auto decoded = DecodePreparedStructureIndex(Bytes(bytes), kInputBytesMost);
  if (!decoded || decoded->Structures.size() != decoded->Surfaces.size()) {
    Fail("invalid prepared index");
  }
  const auto base = std::make_shared<PreparedStructureTile>(std::move(*decoded));
  const Frame frame{.Axes = {.East = Vector(request.at("east")),
                             .North = Vector(request.at("north")),
                             .Up = Vector(request.at("up"))},
                    .Origin = Vector(request.at("origin"))};
  const auto cells = request.at("cells").get<std::unordered_set<uint32_t>>();
  SourceCosts costs;
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
  const auto atlas = CapturePlaneViews(*producer,
                                       frame,
                                       metadata,
                                       *projection,
                                       pixels,
                                       request.at("source_tests").get<uint64_t>(),
                                       report);
  const auto encoded = atlas.Encode("native-source-planes-impostor-1", error);
  if (!encoded) { Fail(error); }
  Write<uint8_t>(output / "atlas.bin", *encoded);
  SaveFields(atlas, output);
  report.update(
      {{"form_reads", costs.FormReads},
       {"form_bytes", costs.FormBytes},
       {"atlas_bytes", encoded->size()},
       {"pixels", pixels},
       {"elapsed_ms", std::chrono::duration<double, std::milli>(Clock::now() - began).count()},
       {"scope",
        "Source index and lazy analytical forms only. No native fine/shell mesher, "
        "world/SSE/occlusion admission or device frame-budget claim."}});
  std::ofstream reportFile(output / "source-result.json");
  reportFile << report.dump(2) << '\n';
  if (!reportFile) { Fail("report write failed"); }
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { Fail("expected original-input.json request.json new-output-directory"); }
  const Fs::path output(argv[3]);
  if (Fs::exists(output) || !Fs::create_directories(output)) {
    Fail("new output directory required");
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) { Fail(SDL_GetError()); }
  Run(argv[1], Document(argv[2]), output);
  SDL_Quit();
  return 0;
}
