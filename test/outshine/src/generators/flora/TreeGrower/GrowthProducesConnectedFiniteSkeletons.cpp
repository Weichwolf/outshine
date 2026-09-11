#include "TreeGrower.h"
#include "TreeMesher.h"
#include "ModelLadder.h"
#include <scene/Geometry.h>
#include <scene/Material.h>
#include <export/GltfExporter.h>
#include "Check.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using outshine::Vec3f;
using outshine::Generators::TreeSkeleton;

template <typename T>
  requires std::is_arithmetic_v<T>
void Write(std::ofstream &out, T value) {
  out.write(reinterpret_cast<const char *>(&value), sizeof value);
}

void Write(std::ofstream &out, const Vec3f &value) {
  for (int axis = 0; axis < 3; ++axis) { Write(out, value[axis]); }
}

void Snapshot(std::ofstream &out, const TreeSkeleton &tree) {
  Write(out, tree.Seed);
  Write(out, tree.Nodes.size());
  Write(out, tree.Shoots.size());
  Write(out, tree.LeafPoints.size());
  Write(out, tree.BoxMin);
  Write(out, tree.BoxMax);
  Write(out, tree.FootRadius);
  Write(out, tree.DbhRadius);
  for (const auto &node : tree.Nodes) {
    Write(out, node.Pos);
    Write(out, node.Dir);
    Write(out, node.Up);
    Write(out, node.Radius);
  }
  for (const auto &shoot : tree.Shoots) {
    Write(out, shoot.Parent);
    Write(out, shoot.ParentNode);
    Write(out, shoot.Roll);
    Write(out, shoot.First);
    Write(out, shoot.Count);
    Write(out, shoot.Sides);
    Write(out, static_cast<uint8_t>(shoot.End));
    Write(out, shoot.Reach);
  }
  for (const auto &leaf : tree.LeafPoints) {
    Write(out, leaf.Pos);
    Write(out, leaf.Dir);
  }
}

void ExportBark(const TreeSkeleton &tree) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  TreeMesh mesh;
  TreeMesher mesher;
  mesher.Draw(tree, ModelLadder::Error(0), mesh);
  Geometry geometry;
  Material material;
  material.Unlit = true;
  material.BaseColour = {{0.75f, 0.5f, 0.24f, 1}};
  const auto surface = geometry.addSurface("diagnostic bark silhouette", material);
  CHECK(surface.has_value(), "diagnostic material is native");
  if (!surface) { return; }
  const int part = geometry.addPart("fir branches", *surface);
  std::vector<float> positions;
  std::vector<float> normals;
  for (size_t at = 0; at < mesh.BarkVertexCount(); ++at) {
    for (size_t axis = 0; axis < 3; ++axis) {
      positions.push_back(mesh.BarkVerts[at * TreeMesh::kBarkFloats + axis]);
      normals.push_back(mesh.BarkVerts[at * TreeMesh::kBarkFloats + axis + 3]);
    }
  }
  CHECK(geometry.setPositions(part, positions) && geometry.setNormals(part, normals) &&
            geometry.setTriangles(part, mesh.BarkIdx),
        "diagnostic mesh uses the public geometry API");
  const auto glb = exportGlb(geometry);
  CHECK(glb.has_value(), "diagnostic mesh exports through the public GLB API");
  if (!glb) { return; }
  std::ofstream file(std::filesystem::temp_directory_path() / "outshine-grower-fir.glb",
                     std::ios::binary);
  file.write(reinterpret_cast<const char *>(glb->data()),
             static_cast<std::streamsize>(glb->size()));
  file.close();
  CHECK(!file.fail(), "diagnostic GLB is available to outshine-client");
}

bool Finite(const Vec3f &value) {
  return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

void NearlyParallelFramesRemainOrthogonal() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const Vec3f along = DirectionOrUp(Vec3f{{0.2f, 0.7f, -0.3f}});
  for (float offset : {0.0f, 1e-7f, 1e-6f, 1e-5f, 1e-4f, 1e-3f}) {
    for (float sign : {-1.0f, 1.0f}) {
      const auto frame =
          FrameFrom({.Along = along, .Reference = along * sign + Vec3f{{offset, 0, 0}}});
      CHECK(Finite(frame.Normal) && Finite(frame.Binormal) &&
                std::abs(Dot(along, frame.Normal)) < 1e-6 &&
                std::abs(Dot(along, frame.Binormal)) < 1e-6 &&
                std::abs(Dot(frame.Normal, frame.Binormal)) < 1e-6 &&
                std::abs(Dot(frame.Normal, frame.Normal) - 1) < 1e-6 &&
                std::abs(Dot(frame.Binormal, frame.Binormal) - 1) < 1e-6 &&
                Dot(Cross(along, frame.Normal), frame.Binormal) > 0.99999f,
            "parallel and nearly parallel references produce a right-handed orthonormal frame");
    }
  }
}

bool Connected(const TreeSkeleton &tree) {
  size_t covered = 0;
  for (size_t at = 0; at < tree.Shoots.size(); ++at) {
    const auto &shoot = tree.Shoots[at];
    if (shoot.Count < 0 || shoot.First < 0) { return false; }
    if (shoot.Count > 0) {
      if (static_cast<size_t>(shoot.First) != covered ||
          static_cast<size_t>(shoot.Count) > tree.Nodes.size() - covered) {
        return false;
      }
      covered += static_cast<size_t>(shoot.Count);
    }
    if (shoot.Parent < 0) { continue; }
    if (static_cast<size_t>(shoot.Parent) >= at) { return false; }
    const auto &parent = tree.Shoots[static_cast<size_t>(shoot.Parent)];
    if (shoot.ParentNode < parent.First || shoot.ParentNode >= parent.First + parent.Count) {
      return false;
    }
  }
  return covered == tree.Nodes.size();
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  NearlyParallelFramesRemainOrthogonal();
  std::vector<std::filesystem::path> paths;
  for (const auto &entry : std::filesystem::directory_iterator("src/assets/world/species")) {
    if (entry.path().extension() == ".json") { paths.push_back(entry.path()); }
  }
  std::ranges::sort(paths);
  CHECK(!paths.empty(), "the independent species corpus is present");
  std::ofstream snapshot(std::filesystem::temp_directory_path() / "outshine-grower-skeletons.bin",
                         std::ios::binary);
  CHECK(snapshot.good(), "system-temp snapshot opens for before/after comparison");
  TreeGrower grower;
  TreeSkeleton tree;
  for (const auto &path : paths) {
    std::ifstream file(path);
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    TreeSpecies species;
    CHECK(species.Parse(source.data(), source.size()), path.filename().string().c_str());
    grower.Grow(species, tree);
    CHECK(!tree.Nodes.empty() && Connected(tree),
          "shoots own contiguous nodes and valid earlier parents");
    CHECK(Finite(tree.BoxMin) && Finite(tree.BoxMax), "normalization publishes finite bounds");
    CHECK(std::ranges::all_of(tree.Nodes,
                              [](const auto &node) {
                                return Finite(node.Pos) && Finite(node.Dir) && Finite(node.Up) &&
                                       std::isfinite(node.Radius) && node.Radius > 0 &&
                                       std::abs(Dot(node.Dir, node.Dir) - 1) < 1e-4 &&
                                       std::abs(Dot(node.Up, node.Up) - 1) < 1e-4 &&
                                       std::abs(Dot(node.Dir, node.Up)) < 1e-4;
                              }),
          "node frames are finite orthonormal frames with positive radii");
    CHECK(std::ranges::all_of(tree.LeafPoints,
                              [](const auto &leaf) {
                                return Finite(leaf.Pos) && Finite(leaf.Dir) &&
                                       std::abs(Dot(leaf.Dir, leaf.Dir) - 1) < 1e-4;
                              }),
          "foliage attachment directions are finite unit vectors");
    if (path.stem() == "fir") { ExportBark(tree); }
    Snapshot(snapshot, tree);
    Write(snapshot, grower.Passes());
    Write(snapshot, grower.DbhErrorRel());
    Write(snapshot, grower.GrowHeight());
  }
  const std::string pole =
      R"({"name":"pole","trunk_steps":3,"wander":0,"max_order":0,"terminal_fork":0,"foliate":0,"leader_splay":0})";
  TreeSpecies species;
  CHECK(species.Parse(pole.data(), pole.size()), "analytical single-stem control parses");
  grower.Grow(species, tree);
  CHECK(tree.Nodes.size() == 4 && tree.Shoots.size() == 1 && tree.LeafPoints.empty(),
        "grower reuse clears all previous branches and foliage");
  CHECK(std::ranges::all_of(tree.Nodes,
                            [](const auto &node) {
                              return node.Dir == Vec3f{{0, 1, 0}} && node.Pos[0] == 0 &&
                                     node.Pos[2] == 0;
                            }),
        "zero wandering and splay produce a straight vertical stem");
  Snapshot(snapshot, tree);
  const std::string branches =
      R"({"name":"branches","seed":1,"trunk_steps":3,"wander":0,"max_order":1,"terminal_fork":0,"branch_chance":1,"min_radius":0.00001,"order_radius":0.5,"bole_frac":0,"order_len":1,"foliate":0,"crown":"free"})";
  CHECK(species.Parse(branches.data(), branches.size()), "deterministic branch control parses");
  grower.Grow(species, tree);
  CHECK(tree.Shoots.size() == 4 && tree.Nodes.size() == 19 && Connected(tree),
        "one four-node stem plus three five-node shoots drains dynamically appended work");
  CHECK(tree.LeafPoints.empty(), "disabling foliage is independent of branch processing");
  snapshot.close();
  CHECK(!snapshot.fail(), "complete snapshot writes successfully");
  return Report();
}
