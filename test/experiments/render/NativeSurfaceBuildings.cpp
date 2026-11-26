#include "AssetCache.h"
#include "BuildingGeometrySource.h"
#include "PreparedBuildingAssets.h"
#include "RawSourceCache.h"
#include "ReadTextFile.h"
#include "SourceSet.h"
#include "StructureArtifact.h"
#include "SurfaceTile.h"
#include "TangentFrame.h"
#include "WriteFileAtomically.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using Json = nlohmann::json;
constexpr size_t kRequestBytesMost = size_t{16} * 1024 * 1024;

bool AppendPart(const GeometryAsset::Product &product,
                const TangentFrame &tile,
                int part,
                Json &output) {
  const auto &mesh = product.Mesh;
  const auto points = mesh.positionsOf(part);
  const auto normals = mesh.normalsOf(part);
  const auto colours = mesh.coloursOf(part);
  const auto indices = mesh.trianglesOf(part);
  auto &positions = output["vertices"];
  auto &triangles = output["indices"];
  const auto start = positions.size();
  if (points.size() != normals.size()) { return false; }
  const auto &tint = mesh.surfaceAt(mesh.materialOf(part)).BaseColour;
  const auto placement = product.Layout.WorldFromLocal * mesh.placementOf(part);
  for (size_t at = 0; at < points.size(); at += 3) {
    const auto p = tile.ToLocalPosition(
        placement.TransformPoint({{points[at], points[at + 1], points[at + 2]}}));
    const auto n = tile.ToLocalDirection(
        placement.TransformDirection({{normals[at], normals[at + 1], normals[at + 2]}}));
    Vec3 unit{{n.EastM, n.UpM, -n.NorthM}};
    if (!Normalise(unit)) { return false; }
    positions.push_back(
        {static_cast<float>(p.EastM), static_cast<float>(p.UpM), static_cast<float>(-p.NorthM)});
    output["normals"].push_back(unit.Axis);
    std::array<unsigned, 4> rgba{0, 0, 0, UINT8_MAX};
    for (size_t channel = 0; channel < 3; ++channel) {
      const float colour = colours.empty() ? 1.0f : colours[at / 3 * rgba.size() + channel];
      rgba[channel] = static_cast<unsigned>(
          std::lround(UINT8_MAX * std::clamp(tint[channel] * colour, 0.0f, 1.0f)));
    }
    output["colours"].push_back(rgba);
  }
  for (const uint32_t index : indices) { triangles.push_back(start + index); }
  return true;
}

bool ValidRequest(const Json &request) {
  if (!request.is_object() || !request.contains("origin") || !request.contains("shell") ||
      !request.contains("sources")) {
    return false;
  }
  const auto &origin = request["origin"];
  const auto within = [&](const char *name, size_t limit) {
    const auto value = request.find(name);
    return value != request.end() && value->is_number_unsigned() && value->get<uint64_t>() <= limit;
  };
  return origin.is_array() && origin.size() == 2 && origin[0].is_number() &&
         origin[1].is_number() && std::abs(origin[0].get<double>()) <= 180 &&
         std::abs(origin[1].get<double>()) <= 90 && request["shell"].is_boolean() &&
         request["sources"].is_object() && within("vertices_left", SurfaceTile::MaximumVertices) &&
         within("indices_left", SurfaceTile::MaximumIndices);
}

bool ValidSources(const Json &group) {
  if (!group.is_array() || group.size() != 2 || !group[0].is_number_unsigned() ||
      !group[1].is_array() || group[1].empty()) {
    return false;
  }
  return std::ranges::all_of(group[1], [](const Json &item) {
    return item.is_number_unsigned() && item.get<uint64_t>() <= UINT32_MAX;
  });
}

struct TileCapacity {
  size_t Vertices;
  size_t Indices;
};

std::expected<std::optional<Json>, std::string> BakeTile(AssetCache &geometry,
                                                         PreparedBuildingAssets &sources,
                                                         const Json &request,
                                                         LevelOfDetail detail) {
  constexpr BuildingGeometrySource::Budget budget{.EncodedBytesMost = kStructureArtifactBytesMost,
                                                  .TriangleAttempts = SurfaceTile::MaximumIndices};
  const auto &origin = request["origin"];
  const auto frame = TangentFrame::At(
      {.LongitudeDeg = origin[0].get<double>(), .LatitudeDeg = origin[1].get<double>()});
  TileCapacity left{.Vertices = request["vertices_left"].get<size_t>(),
                    .Indices = request["indices_left"].get<size_t>()};
  Json output{{"vertices", Json::array()},
              {"normals", Json::array()},
              {"indices", Json::array()},
              {"colours", Json::array()}};
  for (const auto &[key, group] : request["sources"].items()) {
    if (!ValidSources(group)) { return std::unexpected("invalid building group source list"); }
    const BuildingGeometrySource::Recipe recipe{.BaseKey = key,
                                                .SourceCount = group[0].get<uint64_t>(),
                                                .Sources = group[1].get<std::vector<uint32_t>>(),
                                                .Detail = detail};
    const auto product = BuildingGeometrySource::Resolve(geometry, &sources, recipe, budget);
    if (!product) { return std::unexpected(key + ": " + product.error()); }
    for (int part = 0; part < product->Mesh.parts(); ++part) {
      const auto vertices = product->Mesh.positionsOf(part).size() / 3;
      const auto indices = product->Mesh.trianglesOf(part).size();
      if (vertices > left.Vertices || indices > left.Indices) { return std::nullopt; }
      left.Vertices -= vertices;
      left.Indices -= indices;
      if (!AppendPart(*product, frame, part, output)) {
        return std::unexpected("invalid building group vertex streams");
      }
    }
  }
  return output;
}

std::expected<void, std::string> ExportTile(AssetCache &geometry,
                                            PreparedBuildingAssets &sources,
                                            const Json &request,
                                            const std::filesystem::path &path) {
  if (!ValidRequest(request)) { return std::unexpected("invalid tile request"); }
  const bool shell = request["shell"].get<bool>();
  auto baked =
      BakeTile(geometry, sources, request, shell ? LevelOfDetail::Shell : LevelOfDetail::Fine);
  if (baked && !*baked && !shell) {
    baked = BakeTile(geometry, sources, request, LevelOfDetail::Shell);
  }
  if (!baked) { return std::unexpected(baked.error()); }
  if (!*baked) { return std::unexpected("building envelopes exceed the tile capacity"); }
  const auto text = (**baked).dump();
  return WriteFileAtomically(path.string(), std::as_bytes(std::span(text)));
}
}

int main(int argc, char *const *argv) {
  if (argc != 5) {
    std::cerr
        << "surface_buildings <source-store> <geometry-store> <requests> <output-directory>\n";
    return 2;
  }
  auto source = AssetCache::Open(argv[1]);
  const auto geometry = AssetCache::Open(argv[2]);
  const auto input = ReadTextFile(argv[3], kRequestBytesMost);
  if (!source || !geometry || !input) { return 1; }
  const Json requests = Json::parse(*input, nullptr, false);
  if (!requests.is_array()) { return 1; }
  Data::RawSourceCache raw({.Directory = std::string(argv[2]) + "-sources"});
  const Data::SourceSet sourceSet(raw);
  const auto assets =
      PreparedBuildingAssets::Open(std::shared_ptr<AssetCache>(std::move(*source)), sourceSet);
  if (!assets) { return 1; }
  const std::filesystem::path output(argv[4]);
  for (size_t at = 0; at < requests.size(); ++at) {
    if (const auto exported =
            ExportTile(**geometry, **assets, requests[at], output / (std::to_string(at) + ".json"));
        !exported) {
      std::cerr << "building group " << at << ": " << exported.error() << '\n';
      return 1;
    }
  }
  return 0;
}
