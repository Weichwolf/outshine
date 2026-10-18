#include "content/AssetCache.h"
#include "StructureArtifact.h"
#include "PreparedStructureCodec.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <system_error>

namespace {
bool ExportMesh(const std::filesystem::path &directory,
                const std::string &name,
                std::span<const outshine::StoredVertex> vertices,
                const outshine::ClusteredMesh &mesh,
                std::span<const uint32_t> fallback) {
  std::ofstream positions(directory / (name + ".positions.f32"), std::ios::binary);
  for (const auto &v : vertices) {
    const std::array<float, 3> point{v.pos[0], v.pos[1], v.pos[2]};
    positions.write(reinterpret_cast<const char *>(point.data()), sizeof(point));
  }
  const auto triangles = mesh.Index.empty() ? fallback : std::span<const uint32_t>(mesh.Index);
  std::ofstream indices(directory / (name + ".indices.u32"), std::ios::binary);
  indices.write(reinterpret_cast<const char *>(triangles.data()),
                static_cast<std::streamsize>(triangles.size_bytes()));
  std::ofstream clusters(directory / (name + ".clusters.csv"));
  clusters << "x,y,z,radius,first,count,level,error,parent_error\n";
  for (const auto &cluster : mesh.Clusters) {
    clusters << std::format("{:.9g},{:.9g},{:.9g},{:.9g},{},{},{},{:.9g},{:.9g}\n",
                            cluster.SelfCenter[0],
                            cluster.SelfCenter[1],
                            cluster.SelfCenter[2],
                            cluster.SelfRadius,
                            cluster.First,
                            cluster.Count,
                            cluster.Level,
                            cluster.SelfErr,
                            cluster.ParentErr);
  }
  positions.flush();
  indices.flush();
  clusters.flush();
  if (!positions || !indices || !clusters) { return false; }
  std::println("{} {} {} {}", name, vertices.size(), triangles.size() / 3, mesh.Clusters.size());
  return true;
}
}

int main(int argc, const char **argv) {
  using namespace outshine;
  if (argc != 4) { return 2; }
  const auto cache = AssetCache::Open(argv[1]);
  if (!cache) {
    std::println(stderr, "cache open {}", static_cast<int>(cache.error()));
    return 2;
  }
  const auto asset = (*cache)->Load(argv[2], Generators::kStructureArtifactBytesMost);
  if (!asset || !*asset || (**asset).Record().Kind != "building-lod") {
    std::println(stderr, "asset load failed");
    return 2;
  }
  const auto tile = Generators::DecodeStructureArtifact((**asset).Bytes(), argv[2], 0);
  if (!tile) {
    std::println(stderr, "structure decode failed");
    return 2;
  }
  const auto parent =
      (*cache)->Load((**asset).Record().Parent, Generators::kStructureArtifactBytesMost);
  if (!parent || !*parent) {
    std::println(stderr, "building parent is missing");
    return 2;
  }
  const auto basis =
      Generators::DecodePreparedStructureIndex((**parent).Bytes(), size_t{128} * 1024 * 1024);
  if (!basis) {
    std::println(stderr, "building index decode failed");
    return 2;
  }
  const std::filesystem::path output(argv[3]);
  std::error_code error;
  std::filesystem::create_directories(output, error);
  if (error ||
      !ExportMesh(output, "walls", tile->Built.WallCorners, tile->Walls, tile->Built.WallRun) ||
      !ExportMesh(output, "roofs", tile->Built.RoofCorners, tile->Roofs, tile->Built.RoofRun)) {
    return 2;
  }
  std::ofstream metadata(output / "scene.json");
  const auto anchor = basis->AnchorEcef;
  metadata << std::format(R"({{"asset":"{}","parent":"{}","anchor_ecef":[{:.17g},{:.17g},{:.17g}])",
                          argv[2],
                          (**asset).Record().Parent,
                          anchor[0],
                          anchor[1],
                          anchor[2]);
  if (tile->SelectedView && tile->SelectedView->EyeEcef) {
    const auto eye = *tile->SelectedView->EyeEcef;
    metadata << std::format(
        R"(,"eye_ecef":[{:.17g},{:.17g},{:.17g}],"focal_px":{:.17g},"allowed_error_px":{:.17g})",
        eye[0],
        eye[1],
        eye[2],
        tile->SelectedView->Projection.FocalPx,
        tile->SelectedView->Projection.AllowedErrorPx);
  }
  metadata << "}\n";
  metadata.flush();
  return metadata ? 0 : 2;
}
