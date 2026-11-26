#include "Box.h"
#include "GroupImpostorProbeFields.h"
#include "GroupImpostorView.h"
#include "ImpostorBaker.h"
#include "Species.h"
#include "SurfaceTile.h"
#include "TreePrototype.h"
#include "WriteFileAtomically.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
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
using Json = nlohmann::json;
using CaptureInput = std::expected<ImpostorCapture, std::string>;

ImpostorCapture WithBounds(const Box &bounds) {
  return {.Sources = {}, .LeastM = bounds.Min, .MostM = bounds.Max, .ViewDirections = {}};
}

CaptureInput SurfaceOverlay(const SurfaceTile &tile, bool doubleSided) {
  const size_t first = static_cast<size_t>(tile.GridSide) * tile.GridSide;
  if (first == 0 || first >= tile.Vertices.size() || tile.GroundIndices >= tile.Indices.size()) {
    return std::unexpected("building tile requires a packed overlay");
  }
  Geometry geometry;
  Material matte;
  matte.BaseColour = {{1, 1, 1, 1}};
  matte.DoubleSided = doubleSided;
  const auto surface = geometry.addSurface("building", matte);
  if (!surface) { return std::unexpected("could not register building material"); }
  const auto part = geometry.addPart("building group", *surface);
  if (!part) { return std::unexpected("could not register building geometry"); }
  Geometry::Mesh mesh;
  Box bounds;
  const auto colourFirst = tile.Vertices.size() - tile.Colours.size();
  if (first < colourFirst) { return std::unexpected("building overlay has no material colours"); }
  for (size_t at = first; at < tile.Vertices.size(); ++at) {
    const auto &v = tile.Vertices[at];
    mesh.PositionsM.insert(mesh.PositionsM.end(), v.PositionM.begin(), v.PositionM.end());
    mesh.Normals.insert(mesh.Normals.end(), v.Normal.begin(), v.Normal.end());
    bounds.Cover(Vec3{{v.PositionM[0], v.PositionM[1], v.PositionM[2]}});
    const auto &rgb = tile.Colours[at - colourFirst];
    for (size_t channel = 0; channel < 3; ++channel) {
      mesh.Colours.push_back(static_cast<float>(rgb[channel]) / UINT8_MAX);
    }
    mesh.Colours.push_back(1);
  }
  for (size_t at = tile.GroundIndices; at < tile.Indices.size(); ++at) {
    mesh.Indices.push_back(tile.Indices[at] - static_cast<uint32_t>(first));
  }
  if (!geometry.adoptMesh(*part, mesh)) { return std::unexpected("invalid group geometry"); }
  auto capture = WithBounds(bounds);
  capture.Sources.push_back({.Mesh = std::move(geometry), .Instances = {Mat4{}}});
  return capture;
}

CaptureInput Trees(const char *speciesPath, int rows, std::vector<Vec3> &roots) {
  using namespace outshine::Generators;
  TreeSpecies species;
  if (!ReadSpecies(speciesPath, &species)) { return std::unexpected("invalid tree species"); }
  const auto tree = TreePrototype::Grow(species);
  if (!tree || tree->Ranks().empty()) { return std::unexpected("could not grow a tree"); }
  auto geometry = tree->InstancedGeometryAt(0);
  if (!geometry || geometry->Placements.empty()) {
    return std::unexpected("tree lacks instanced foliage");
  }
  const auto span = geometry->MostM - geometry->LeastM;
  constexpr double kOverlap = 0.2;
  const double space = std::max(span[0], span[2]) * (1.0 - kOverlap);
  std::vector<Mat4> stems;
  std::vector<Mat4> crowns;
  Box bounds;
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < rows; ++col) {
      const Vec3 root{{(col + (row % 2) * 0.5) * space, 0, row * space * std::sqrt(0.75)}};
      Mat4 translation;
      translation.SetTranslation(root);
      roots.push_back(root);
      stems.push_back(translation);
      for (const auto &leaf : geometry->Placements) { crowns.push_back(translation * leaf); }
      bounds.Cover(root + geometry->LeastM);
      bounds.Cover(root + geometry->MostM);
    }
  }
  auto capture = WithBounds(bounds);
  capture.Sources.push_back({.Mesh = std::move(geometry->Bark), .Instances = std::move(stems)});
  capture.Sources.push_back({.Mesh = std::move(geometry->Leaf), .Instances = std::move(crowns)});
  return capture;
}

bool Views(ImpostorCapture &capture, const char *path) {
  std::ifstream file(path);
  Vec3 direction;
  while (file >> direction[0]) {
    constexpr size_t kMaximumViews = 64;
    if (!(file >> direction[1] >> direction[2]) || capture.ViewDirections.size() == kMaximumViews ||
        !Normalise(direction)) {
      return false;
    }
    capture.ViewDirections.push_back(direction);
  }
  return file.eof() && !capture.ViewDirections.empty();
}

template <class T> std::optional<T> PositiveArgument(std::string_view text) {
  T value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value <= 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<uint32_t> CardSide(std::span<const char *const> arguments, int pixels) {
  if (arguments.size() < 7) { return 0; }
  const auto side = PositiveArgument<uint32_t>(arguments[6]);
  if (!side || !GroupImpostorView::ValidSide(*side) || static_cast<uint32_t>(pixels) % *side != 0) {
    return std::nullopt;
  }
  return side;
}

std::optional<size_t> CardViews(std::span<const char *const> arguments, size_t available) {
  if (arguments.size() < 7) { return 0; }
  if (arguments.size() == 7) { return available; }
  const auto views = PositiveArgument<size_t>(arguments[7]);
  if (!views || *views < 2 || *views > available) { return std::nullopt; }
  return views;
}

size_t SourceTriangles(const ImpostorCapture &capture) {
  size_t triangles = 0;
  for (const auto &source : capture.Sources) {
    for (int part = 0; part < source.Mesh.parts(); ++part) {
      triangles += source.Mesh.trianglesOf(part).size() / 3 * source.Instances.size();
    }
  }
  return triangles;
}

int Run(int argc, const char *const *argv) {
  const std::string_view kind = argv[1];
  if (kind != "buildings" && kind != "overlay" && kind != "stand" && kind != "tree") { return 2; }
  const std::span arguments(argv, static_cast<size_t>(argc));
  const auto pixels = PositiveArgument<int>(argv[4]);
  if (!pixels) { return 2; }
  const auto cardSide = CardSide(arguments, *pixels);
  if (!cardSide) { return 2; }
  constexpr int kStandRows = 5;
  std::vector<Vec3> roots;
  const auto began = std::chrono::steady_clock::now();
  std::optional<SurfaceTile> tile;
  if (kind == "buildings" || kind == "overlay") {
    auto loaded = ReadSurfaceTile(argv[2]);
    if (!loaded) {
      std::cerr << loaded.error() << '\n';
      return 1;
    }
    tile = std::move(*loaded);
  }
  auto capture = tile ? SurfaceOverlay(*tile, kind == "overlay")
                      : Trees(argv[2], kind == "tree" ? 1 : kStandRows, roots);
  if (!capture) {
    std::cerr << capture.error() << '\n';
    return 1;
  }
  if (!Views(*capture, argv[3])) { return 2; }
  const auto cardViews = CardViews(arguments, capture->ViewDirections.size());
  if (!cardViews) { return 2; }
  const size_t triangles = SourceTriangles(*capture);
  const auto start = std::chrono::steady_clock::now();
  std::string error;
  const ImpostorAtlasShape shape{.Pixels = *pixels,
                                 .Views = static_cast<unsigned>(capture->ViewDirections.size())};
  const auto atlas = ImpostorBaker::Bake(std::move(*capture), shape, error);
  if (!atlas) {
    std::cerr << error << '\n';
    return 1;
  }
  const auto writtenAt = std::chrono::steady_clock::now();
  const std::filesystem::path directory(argv[5]);
  auto directions = Json::array();
  for (size_t at = 0; at < atlas->Views().size(); ++at) {
    const auto &view = atlas->Views()[at];
    directions.push_back(view.TowardEye.Axis);
    if (!GroupProbe::SaveView(directory / (std::to_string(at) + ".field"), view)) { return 1; }
    if (at < *cardViews &&
        !GroupProbe::SaveCard(directory / (std::to_string(at) + ".card"), *atlas, at, *cardSide)) {
      return 1;
    }
  }
  auto rootPositions = Json::array();
  for (const auto &root : roots) { rootPositions.push_back(root.Axis); }
  Json info{{"pixels", *pixels},
            {"card_side", *cardSide},
            {"card_views", *cardViews},
            {"centre_m", atlas->CentreM().Axis},
            {"half_extent_m", atlas->HalfExtentM()},
            {"directions", std::move(directions)},
            {"roots_m", std::move(rootPositions)},
            {"source_triangles", triangles},
            {"source_ms", std::chrono::duration<double, std::milli>(start - began).count()},
            {"capture_ms", std::chrono::duration<double, std::milli>(writtenAt - start).count()},
            {"write_ms",
             std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - writtenAt)
                 .count()}};
  if (tile) {
    info["surface"] = {{"coverage", {tile->Coverage.Zoom, tile->Coverage.X, tile->Coverage.Y}},
                       {"origin_lon_lat", {tile->Origin.LongitudeDeg, tile->Origin.LatitudeDeg}},
                       {"revision", tile->Revision}};
  }
  const auto text = info.dump(2) + '\n';
  return WriteFileAtomically((directory / "group.json").string(), std::as_bytes(std::span(text)))
                 .has_value()
             ? 0
             : 1;
}
}

int main(int argc, const char *const *argv) {
  if (argc < 6 || argc > 8) {
    std::cerr << "group_impostors <buildings|overlay|stand|tree> <source> <directions> <pixels> "
                 "<directory> "
                 "[card pixels [card views]]\n";
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << SDL_GetError() << '\n';
    return 1;
  }
  const int status = Run(argc, argv);
  SDL_Quit();
  return status;
}
