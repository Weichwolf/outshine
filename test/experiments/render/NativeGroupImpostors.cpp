#include "Box.h"
#include "ByteArchive.h"
#include "GroupImpostorBake.h"
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

CaptureInput BuildingTile(std::string_view path) {
  const auto tile = ReadSurfaceTile(path);
  if (!tile) { return std::unexpected(tile.error()); }
  const size_t first = static_cast<size_t>(tile->GridSide) * tile->GridSide;
  if (first == 0 || first >= tile->Vertices.size() || tile->GroundIndices >= tile->Indices.size()) {
    return std::unexpected("building tile requires a packed overlay");
  }
  Geometry geometry;
  Material matte;
  matte.BaseColour = {{1, 1, 1, 1}};
  const auto surface = geometry.addSurface("building", matte);
  if (!surface) { return std::unexpected("could not register building material"); }
  const auto part = geometry.addPart("building group", *surface);
  if (!part) { return std::unexpected("could not register building geometry"); }
  Geometry::Mesh mesh;
  Box bounds;
  const auto colourFirst = tile->Vertices.size() - tile->Colours.size();
  if (first < colourFirst) { return std::unexpected("building overlay has no material colours"); }
  for (size_t at = first; at < tile->Vertices.size(); ++at) {
    const auto &v = tile->Vertices[at];
    mesh.PositionsM.insert(mesh.PositionsM.end(), v.PositionM.begin(), v.PositionM.end());
    mesh.Normals.insert(mesh.Normals.end(), v.Normal.begin(), v.Normal.end());
    bounds.Cover(Vec3{{v.PositionM[0], v.PositionM[1], v.PositionM[2]}});
    const auto &rgb = tile->Colours[at - colourFirst];
    for (size_t channel = 0; channel < 3; ++channel) {
      mesh.Colours.push_back(static_cast<float>(rgb[channel]) / UINT8_MAX);
    }
    mesh.Colours.push_back(1);
  }
  for (size_t at = tile->GroundIndices; at < tile->Indices.size(); ++at) {
    mesh.Indices.push_back(tile->Indices[at] - static_cast<uint32_t>(first));
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

bool SaveView(const std::filesystem::path &path, const ImpostorAtlas::View &view) {
  constexpr size_t kTexelBytes = sizeof(uint32_t) + 9 * sizeof(float);
  ByteWriter output(view.Texels.size() * kTexelBytes);
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    const auto &texel = view.Texels[at];
    const auto &material = view.Materials[at];
    if (!output.Number(texel.Surface) || !output.Number(texel.Depth)) { return false; }
    for (const float component : texel.Normal) {
      if (!output.Number(component)) { return false; }
    }
    for (const float component : material.BaseColour) {
      if (!output.Number(component)) { return false; }
    }
    if (!output.Number(material.Roughness) || !output.Number(material.Metalness)) { return false; }
  }
  return WriteFileAtomically(path.string(), std::as_bytes(output.Bytes())).has_value();
}

bool SaveCard(const std::filesystem::path &path,
              const ImpostorAtlas &atlas,
              size_t view,
              uint32_t side) {
  const auto card = BakeGroupImpostorView(atlas, view, side);
  if (!card) { return false; }
  const auto bytes = EncodeGroupImpostorView(*card);
  if (!bytes || !DecodeGroupImpostorView(*bytes)) { return false; }
  return WriteFileAtomically(path.string(), std::as_bytes(std::span(*bytes))).has_value();
}

int Run(int argc, const char *const *argv) {
  const std::string_view kind = argv[1];
  if (kind != "buildings" && kind != "stand" && kind != "tree") { return 2; }
  int pixels = 0;
  const std::string_view size = argv[4];
  const auto number = std::from_chars(size.data(), size.data() + size.size(), pixels);
  if (number.ec != std::errc{} || number.ptr != size.data() + size.size()) { return 2; }
  uint32_t cardSide = 0;
  if (argc == 7) {
    const std::string_view side = argv[6];
    const auto parsed = std::from_chars(side.data(), side.data() + side.size(), cardSide);
    if (parsed.ec != std::errc{} || parsed.ptr != side.data() + side.size() ||
        !GroupImpostorView::ValidSide(cardSide) || pixels <= 0 ||
        static_cast<uint32_t>(pixels) % cardSide != 0) {
      return 2;
    }
  }
  constexpr int kStandRows = 5;
  std::vector<Vec3> roots;
  const auto began = std::chrono::steady_clock::now();
  auto capture = kind == "buildings" ? BuildingTile(argv[2])
                                     : Trees(argv[2], kind == "tree" ? 1 : kStandRows, roots);
  if (!capture) {
    std::cerr << capture.error() << '\n';
    return 1;
  }
  if (!Views(*capture, argv[3])) { return 2; }
  size_t triangles = 0;
  for (const auto &source : capture->Sources) {
    for (int part = 0; part < source.Mesh.parts(); ++part) {
      triangles += source.Mesh.trianglesOf(part).size() / 3 * source.Instances.size();
    }
  }
  const auto start = std::chrono::steady_clock::now();
  std::string error;
  const ImpostorAtlasShape shape{.Pixels = pixels,
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
    if (!SaveView(directory / (std::to_string(at) + ".field"), view)) { return 1; }
    if (cardSide > 0 &&
        !SaveCard(directory / (std::to_string(at) + ".card"), *atlas, at, cardSide)) {
      return 1;
    }
  }
  auto rootPositions = Json::array();
  for (const auto &root : roots) { rootPositions.push_back(root.Axis); }
  const Json info{
      {"pixels", pixels},
      {"card_side", cardSide},
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
  const auto text = info.dump(2) + '\n';
  return WriteFileAtomically((directory / "group.json").string(), std::as_bytes(std::span(text)))
                 .has_value()
             ? 0
             : 1;
}
}

int main(int argc, const char *const *argv) {
  if (argc != 6 && argc != 7) {
    std::cerr
        << "group_impostors <buildings|stand|tree> <source> <directions> <pixels> <directory> "
           "[card pixels]\n";
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
