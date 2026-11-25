#include "BinaryValueArchive.h"
#include "Box.h"
#include "FootprintTerraces.h"
#include "Geometry.h"
#include "ImageCapture.h"
#include "ImpostorBaker.h"
#include "ImpostorPoseBounds.h"
#include "ImpostorSurface.h"
#include "PrismGrid.h"
#include "PrismInstance.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TangentFrame.h"
#include "UrbanPrismCodec.h"
#include "Viewing.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using Clock = std::chrono::steady_clock;
constexpr Extent kFrame{.WidthPx = 1280, .HeightPx = 720};
constexpr size_t kFrames = 120, kInputBytesMost = size_t{128} * 1024 * 1024;
constexpr size_t kP99Index = (99 * kFrames + 99) / 100 - 1;
constexpr double kErrorPx = 4;

struct Flight {
  Vec3 Start, End, Forward, Up;
  double Fov = 0;
};

struct FootprintGroup {
  Geometry Mesh;
  Box Bounds;
};

struct PrismRows {
  std::shared_ptr<PrismGrid> Grid;
  std::vector<PrismInstance> Instances;
};

struct Replay {
  Geometry Mesh;
  std::optional<PrismRows> Prisms;
};

struct Counts {
  size_t Groups = 0, Cards = 0, AtlasBytes = 0, Triangles = 0;
  size_t BytesRejected = 0, PoseRejected = 0;
};

constexpr Vec4f kWallColour{{.74f, .71f, .65f, 1}};
constexpr Vec4f kRoofColour{{.42f, .20f, .14f, 1}};
constexpr float kRoughness = .85f;

std::expected<std::vector<uint8_t>, std::string> Read(const char *path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  const auto size = static_cast<std::streamoff>(in.tellg());
  if (size <= 0 || std::cmp_greater(size, kInputBytesMost)) {
    return std::unexpected("footprint mesh input is missing or too large");
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  in.seekg(0);
  in.read(reinterpret_cast<char *>(bytes.data()), size);
  if (!in) { return std::unexpected("footprint mesh cannot be read"); }
  return bytes;
}

bool ReadPart(BinaryValueReader &in, FootprintGroup &group, std::string_view name, Vec4f colour) {
  Geometry::Mesh mesh;
  if (!in.Array(mesh.PositionsM) || !in.Array(mesh.Normals) || !in.Array(mesh.Indices) ||
      mesh.PositionsM.size() % 3 != 0 || mesh.Normals.size() != mesh.PositionsM.size()) {
    return false;
  }
  if (mesh.Indices.empty()) { return true; }
  for (size_t i = 0; i < mesh.PositionsM.size(); i += 3) {
    group.Bounds.Cover(Vec3{{mesh.PositionsM[i], mesh.PositionsM[i + 1], mesh.PositionsM[i + 2]}});
  }
  Material material;
  material.BaseColour = colour;
  material.Roughness = kRoughness;
  const auto surface = group.Mesh.addSurface(name, material);
  const auto part = surface ? group.Mesh.addPart(name, *surface)
                            : std::expected<int, GeometryPartError>{
                                  std::unexpect, GeometryPartError::CapacityExceeded};
  return part && group.Mesh.adoptMesh(*part, mesh).has_value();
}

Vec3 QuantizedDirection(Vec3 eyeOffset) {
  if (!Normalise(eyeOffset)) { return {}; }
  constexpr double yawStep = std::numbers::pi / 8;
  constexpr double pitchStep = std::numbers::pi * 8 / 180;
  const double yaw = std::round(std::atan2(eyeOffset[0], eyeOffset[2]) / yawStep) * yawStep;
  const double pitch = std::clamp(
      (std::floor(std::asin(std::clamp(eyeOffset[1], -1., 1.)) / pitchStep) + .5) * pitchStep,
      -std::numbers::pi / 2,
      std::numbers::pi / 2);
  const double horizontal = std::cos(pitch);
  return {{std::sin(yaw) * horizontal, std::sin(pitch), std::cos(yaw) * horizontal}};
}

std::optional<Geometry> Card(ImpostorBaker &baker,
                             const FootprintGroup &group,
                             const Flight &flight,
                             ImpostorSurfaceDetail detail,
                             double texelMetres,
                             Counts &counts) {
  const auto direction =
      QuantizedDirection((flight.Start + flight.End) * .5 - group.Bounds.Middle());
  ImpostorCapture capture;
  capture.LeastM = group.Bounds.Min;
  capture.MostM = group.Bounds.Max;
  capture.ViewDirections = {direction};
  std::string error;
  const auto layout = ImpostorBaker::ProjectionFor(capture, {.Pixels = 256, .Views = 1}, error);
  if (!layout) { return std::nullopt; }
  const int pixels =
      std::max(4, static_cast<int>(std::ceil(2 * layout->HalfExtentM / texelMetres)) + 2);
  const ImpostorAtlasShape shape{.Pixels = pixels, .Views = 1};
  const auto bytes = ImpostorAtlas::EncodedBytesMost(shape);
  if (!bytes || *bytes >= group.Mesh.storageBytes()) {
    ++counts.BytesRejected;
    return std::nullopt;
  }
  const auto projection = ImpostorBaker::ProjectionFor(capture, shape, error);
  if (!projection) { return std::nullopt; }
  const auto hull =
      ImpostorPoseBounds::From(projection->Centre, direction, projection->HalfExtentM);
  if (!hull) { return std::nullopt; }
  const Lens lens{.WidePx = kFrame.WidthPx,
                  .HighPx = kFrame.HeightPx,
                  .FovDeg = static_cast<float>(flight.Fov * 180 / std::numbers::pi),
                  .NearM = .05,
                  .FarM = 240000};
  for (const auto eye : {flight.Start, flight.End}) {
    const auto support = hull->ProjectPanoramic(eye, lens);
    if (!support || !support->DepthInside || support->L1UpperPx > kErrorPx) {
      ++counts.PoseRejected;
      return std::nullopt;
    }
  }
  capture.Sources.push_back({.Mesh = group.Mesh.clone(), .Instances = {Mat4{}}});
  const auto atlas = baker.Capture(std::move(capture), shape, error);
  if (!atlas) {
    std::cerr << error << '\n';
    return std::nullopt;
  }
  auto card = BuildImpostorSurface(*atlas, 0, detail);
  if (card) {
    ++counts.Cards;
    counts.AtlasBytes += *bytes;
  }
  return card;
}

bool ReadFlight(BinaryValueReader &in, Flight &flight) {
  return in(flight.Start.Axis, flight.End.Axis, flight.Forward.Axis, flight.Up.Axis, flight.Fov) &&
         std::isfinite(flight.Fov) && flight.Fov > 0 && flight.Fov < std::numbers::pi;
}

std::expected<Geometry, std::string> Prepare(std::span<const uint8_t> bytes,
                                             std::string_view mode,
                                             double texelMetres,
                                             Flight &flight,
                                             Counts &counts) {
  BinaryValueReader in(bytes);
  uint32_t groups = 0;
  if (!ReadFlight(in, flight) || !in(groups) || groups == 0 || groups > 4096) {
    return std::unexpected("invalid footprint flight header");
  }
  Geometry geometry;
  ImpostorBaker baker;
  for (uint32_t i = 0; i < groups; ++i) {
    FootprintGroup group;
    if (!ReadPart(in, group, "walls", kWallColour) || !ReadPart(in, group, "roofs", kRoofColour) ||
        group.Bounds.Empty()) {
      return std::unexpected("invalid footprint group");
    }
    auto card = mode == "geometry" ? std::optional<Geometry>{}
                                   : Card(baker,
                                          group,
                                          flight,
                                          mode == "flat" ? ImpostorSurfaceDetail::Flat
                                                         : ImpostorSurfaceDetail::Depth,
                                          texelMetres,
                                          counts);
    const auto &asset = card ? *card : group.Mesh;
    if (!geometry.append(asset)) {
      return std::unexpected("footprint scene exceeds geometry limits");
    }
  }
  if (in.In.Remaining() != 0) { return std::unexpected("trailing footprint mesh bytes"); }
  counts.Groups = groups;
  for (int part = 0; part < geometry.parts(); ++part) {
    counts.Triangles += geometry.trianglesOf(part).size() / 3;
  }
  return geometry;
}

std::expected<Generators::Osm::UrbanPrisms, std::string>
GeneratePrisms(BinaryValueReader &in, LongitudeLatitude anchor, std::string_view output) {
  uint32_t spacing = 0;
  uint32_t count = 0;
  constexpr size_t footprintHeaderBytes = 2 * sizeof(double) + sizeof(uint32_t);
  if (!in(spacing, count) || count == 0 || count > in.In.Remaining() / footprintHeaderBytes) {
    return std::unexpected("invalid terrace source header");
  }
  std::vector<std::vector<PolygonRing>> rings(count);
  std::vector<Generators::Osm::TerraceFootprint> inputs;
  inputs.reserve(count);
  for (auto &polygon : rings) {
    double bottom = 0;
    double top = 0;
    uint32_t ringCount = 0;
    if (!in(bottom, top, ringCount) || ringCount == 0 ||
        ringCount > in.In.Remaining() / sizeof(uint32_t)) {
      return std::unexpected("invalid terrace footprint header");
    }
    polygon.resize(ringCount);
    for (auto &ring : polygon) {
      uint32_t points = 0;
      if (!in(points) || points < 3 || points > in.In.Remaining() / (2 * sizeof(double))) {
        return std::unexpected("invalid terrace footprint ring");
      }
      ring.resize(points);
      for (auto &point : ring) {
        if (!in(point)) { return std::unexpected("invalid terrace coordinate"); }
      }
    }
    inputs.push_back({.Rings = polygon, .BottomM = bottom, .TopM = top});
  }
  if (in.In.Remaining() != 0) { return std::unexpected("trailing terrace source bytes"); }
  constexpr size_t samplesMost = 8 * (size_t{1} << 20);
  const auto began = Clock::now();
  auto result = Generators::Osm::BuildFootprintTerraces(inputs,
                                                        {.Anchor = anchor,
                                                         .SpacingM = spacing,
                                                         .SamplesMost = samplesMost,
                                                         .PointTestsMost = 4 * samplesMost,
                                                         .Stop = {}});
  const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  if (!result) { return result; }
  std::cout << "native terrace generation " << ms << " ms\n";
  const auto encoded = Generators::Osm::EncodeUrbanPrisms(*result);
  if (!encoded) { return std::unexpected(encoded.error()); }
  std::ofstream asset(std::string(output) + ".asset", std::ios::binary);
  asset.write(reinterpret_cast<const char *>(encoded->data()),
              static_cast<std::streamsize>(encoded->size()));
  if (!asset) { return std::unexpected("could not write native terrace asset"); }
  return result;
}

std::expected<PrismRows, std::string> PreparePrisms(std::span<const uint8_t> bytes,
                                                    Flight &flight,
                                                    Counts &counts,
                                                    std::optional<std::string_view> generate) {
  BinaryValueReader in(bytes);
  LongitudeLatitude anchor;
  if (!ReadFlight(in, flight) || !in(anchor.LatitudeDeg, anchor.LongitudeDeg) ||
      !(std::abs(anchor.LatitudeDeg) <= 90) || !(std::abs(anchor.LongitudeDeg) <= 180)) {
    return std::unexpected("invalid prism flight header");
  }
  const auto frame = TangentFrame::At(anchor);
  auto product = [&] -> std::expected<Generators::Osm::UrbanPrisms, std::string> {
    if (generate) { return GeneratePrisms(in, anchor, *generate); }
    const auto payload = in.In.Take(in.In.Remaining());
    if (!payload) { return std::unexpected("missing prism product"); }
    return Generators::Osm::DecodeUrbanPrisms(*payload);
  }();
  if (!product) { return std::unexpected(product.error()); }
  PrismRows rows{.Grid = std::make_shared<PrismGrid>(std::move(product->Geometry)),
                 .Instances = {}};
  rows.Instances.reserve(product->Placements.size());
  for (uint32_t at = 0; at < product->Placements.size(); ++at) {
    Mat4f placed;
    const auto &transform = product->Placements[at].EcefFromLocal;
    for (size_t axis = 0; axis < 4; ++axis) {
      const Vec3 column{{transform.At(0, axis), transform.At(1, axis), transform.At(2, axis)}};
      const auto local = axis == 3 ? frame.ToLocalPosition(column) : frame.ToLocalDirection(column);
      placed.At(0, axis) = static_cast<float>(local.EastM);
      placed.At(1, axis) = static_cast<float>(local.UpM);
      placed.At(2, axis) = static_cast<float>(-local.NorthM);
    }
    constexpr Vec4f roof{{kRoofColour[0] / kWallColour[0],
                          kRoofColour[1] / kWallColour[1],
                          kRoofColour[2] / kWallColour[2],
                          kRoughness}};
    rows.Instances.push_back(
        {.Patch = at, .Current = placed, .Previous = placed, .TopCoating = roof});
  }
  counts.Groups += rows.Instances.size();
  counts.Triangles += 2 * rows.Instances.size();
  return rows;
}

std::expected<void, std::string> PlacePrisms(SceneRenderer &renderer, const PrismRows &rows) {
  Geometry materials;
  Material surface;
  surface.BaseColour = kWallColour;
  surface.Roughness = kRoughness;
  if (!materials.addSurface("terrace walls", surface)) {
    return std::unexpected("invalid terrace material");
  }
  const auto material = renderer.RegisterPieceMaterials(std::move(materials));
  if (!material) { return std::unexpected(material.error()); }
  const auto placed = renderer.PlacePrismGrid(rows.Grid, rows.Instances, *material);
  if (!placed) { return std::unexpected(placed.error().Message); }
  return {};
}

std::expected<Replay, std::string>
PrepareHybrid(std::span<const uint8_t> bytes, Flight &flight, Counts &counts) {
  BinaryValueReader input(bytes);
  uint64_t meshBytes = 0;
  if (!input(meshBytes) || meshBytes > input.In.Remaining()) {
    return std::unexpected("invalid hybrid footprint header");
  }
  const auto meshPayload = input.In.Take(static_cast<size_t>(meshBytes));
  const auto gridPayload = input.In.Take(input.In.Remaining());
  if (!meshPayload || !gridPayload) { return std::unexpected("incomplete hybrid footprint"); }
  auto mesh = Prepare(*meshPayload, "geometry", 0, flight, counts);
  if (!mesh) { return std::unexpected(mesh.error()); }
  Flight gridFlight;
  auto rows = PreparePrisms(*gridPayload, gridFlight, counts, std::nullopt);
  if (!rows) { return std::unexpected(rows.error()); }
  if (gridFlight.Start != flight.Start || gridFlight.End != flight.End ||
      gridFlight.Forward != flight.Forward || gridFlight.Up != flight.Up ||
      gridFlight.Fov != flight.Fov) {
    return std::unexpected("hybrid footprint components use different flight poses");
  }
  return Replay{.Mesh = std::move(*mesh), .Prisms = std::move(*rows)};
}

std::expected<Replay, std::string> PrepareReplay(const std::vector<uint8_t> &bytes,
                                                 std::string_view mode,
                                                 double texelMetres,
                                                 Flight &flight,
                                                 Counts &counts,
                                                 std::string_view output) {
  if (mode == "hybrid") { return PrepareHybrid(bytes, flight, counts); }
  if (mode == "prisms" || mode == "terraces") {
    const auto generate = mode == "terraces" ? std::optional(output) : std::nullopt;
    auto rows = PreparePrisms(bytes, flight, counts, generate);
    if (!rows) { return std::unexpected(rows.error()); }
    return Replay{.Mesh = {}, .Prisms = std::move(*rows)};
  }
  auto mesh = Prepare(bytes, mode, texelMetres, flight, counts);
  if (!mesh) { return std::unexpected(mesh.error()); }
  return Replay{.Mesh = std::move(*mesh), .Prisms = std::nullopt};
}

bool RenderAt(Core::RuntimeScene &scene, const Flight &flight, double t, std::string &error) {
  const Vec3 at = flight.Start + (flight.End - flight.Start) * t;
  auto camera = Viewpoint::LookAt({.EyeM = at, .AimM = at + flight.Forward}, flight.Up);
  if (!camera) {
    error = "invalid flight pose";
    return false;
  }
  camera->YfovRad = flight.Fov;
  camera->ZNearM = .05;
  camera->ZFarM = 240000;
  scene.Eye(*camera);
  return scene.Draw(error);
}

std::expected<double, std::string> Measure(const Geometry &geometry,
                                           const Flight &flight,
                                           std::string_view output,
                                           const PrismRows *prisms = nullptr) {
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.InitialGeometry = geometry.parts() != 0 ? &geometry : nullptr;
  declaration.SurfaceWidthPx = kFrame.WidthPx;
  declaration.SurfaceHeightPx = kFrame.HeightPx;
  declaration.Outputs = {"surface", "sceneDepth"};
  declaration.KeyLux = 50000;
  declaration.KeyBearingDeg = 210;
  declaration.KeyElevationDeg = 35;
  declaration.IndirectLight = {1000, 1200, 1440};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
    return std::unexpected(error);
  }
  if (prisms != nullptr) {
    const auto placed = PlacePrisms(renderer, *prisms);
    if (!placed) { return std::unexpected(placed.error()); }
  }
  if (!RenderAt(*scene, flight, 0, error)) { return std::unexpected(error); }
  renderer.WaitForGpu();
  const auto start = App::SaveImage(renderer, kFrame, std::string(output) + "-start.png");
  if (!start) { return std::unexpected(start.error()); }
  renderer.ResetMeasurements();
  renderer.KeepGpuSamples(kFrames);
  for (size_t i = 1; i <= kFrames; ++i) {
    if (!RenderAt(*scene, flight, static_cast<double>(i) / kFrames, error)) {
      return std::unexpected(error);
    }
  }
  renderer.WaitForGpu();
  std::vector<double> frameMs;
  for (const auto &sample : renderer.GpuSamples()) { frameMs.push_back(sample.ElapsedMs); }
  if (frameMs.size() != kFrames) { return std::unexpected("incomplete GPU timing samples"); }
  std::ranges::sort(frameMs);
  auto end = App::SaveImage(renderer, kFrame, std::string(output) + "-finish.png");
  if (!end) { return std::unexpected(end.error()); }
  std::vector<float> depth;
  if (renderer.ReadDepth(depth) != ReadState::Ready) {
    return std::unexpected("depth unavailable");
  }
  std::ofstream raw(std::string(output) + ".depth", std::ios::binary);
  raw.write(reinterpret_cast<const char *>(depth.data()),
            static_cast<std::streamsize>(depth.size() * sizeof(float)));
  if (!raw) { return std::unexpected("depth cannot be saved"); }
  return frameMs[kP99Index];
}
}

int main(int argc, const char *const *argv) {
  if (argc < 4 || argc > 5) {
    std::cerr << "usage: footprint_cards PRODUCT geometry|flat|depth|prisms|terraces|hybrid OUTPUT "
                 "[4|8|16]\n";
    return 2;
  }
  const std::string_view mode = argv[2];
  constexpr std::array<std::string_view, 6> modes{
      "geometry", "flat", "depth", "prisms", "terraces", "hybrid"};
  if (!std::ranges::contains(modes, mode)) { return 2; }
  const std::string_view pitch = argc == 5 ? argv[4] : "4";
  int texelMetres = 0;
  const auto parsed = std::from_chars(pitch.begin(), pitch.end(), texelMetres);
  if (parsed.ec != std::errc{} || parsed.ptr != pitch.end() ||
      (texelMetres != 4 && texelMetres != 8 && texelMetres != 16)) {
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << SDL_GetError() << '\n';
    return 1;
  }
  const auto began = Clock::now();
  Flight flight;
  Counts counts;
  const auto data = Read(argv[1]);
  const auto replay = data ? PrepareReplay(*data, mode, texelMetres, flight, counts, argv[3])
                           : std::expected<Replay, std::string>{std::unexpect, data.error()};
  const auto preparation = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  const auto p99 =
      replay ? Measure(replay->Mesh, flight, argv[3], replay->Prisms ? &*replay->Prisms : nullptr)
             : std::expected<double, std::string>{std::unexpect, replay.error()};
  SDL_Quit();
  if (!p99) {
    std::cerr << p99.error() << '\n';
    return 1;
  }
  std::cout << mode << ' ';
  if (mode == "flat" || mode == "depth") { std::cout << texelMetres << " m/texel "; }
  std::string_view triangles = replay->Prisms ? " proxy triangles " : " triangles ";
  if (mode == "hybrid") { triangles = " submitted triangles "; }
  std::cout << counts.Groups << " groups " << counts.Cards << " cards " << counts.Triangles
            << triangles << counts.AtlasBytes << " atlas bytes upper bound " << counts.BytesRejected
            << " byte rejects " << counts.PoseRejected << " pose rejects " << preparation
            << " ms preparation " << *p99 << " ms GPU p99\n";
}
