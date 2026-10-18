#ifndef OUTSHINE_TEST_NATIVE_TREE_DETAIL_PREVIEW_H
#define OUTSHINE_TEST_NATIVE_TREE_DETAIL_PREVIEW_H

#include "content/AssetCache.h"
#include "ImpostorInstances.h"
#include "ImpostorSurface.h"
#include "SceneRenderer.h"
#include "Sha256.h"
#include "StoredVertex.h"
#include "TreePrototype.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <expected>
#include <string_view>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <print>
#include <vector>

namespace outshine::NativeTreeDetailProbe {
inline std::string Directory;
inline std::filesystem::path SelectionPath, DumpPath;

struct Choice {
  size_t Group = 0, Row = 0;
  Mat4 Original;
};

struct Preview {
  Render::SceneRenderer *Renderer = nullptr;
  std::vector<Render::PieceHandle> Pieces;
  std::vector<std::unique_ptr<Render::ImpostorInstances>> Depth;
  std::vector<std::vector<Mat4>> Roots, Far;
  size_t Trees = 0, References = 0, Vertices = 0, ModelBytes = 0;

  void Clear() {
    Depth.clear();
    Roots.clear();
    if (Renderer != nullptr) {
      for (const auto piece : Pieces) { Renderer->ReleasePiece(piece); }
    }
    Pieces.clear();
    Far.clear();
    Renderer = nullptr;
  }
};

// One process and one VegetationStreaming owner are the experiment's explicit scope.
inline Preview Held;

inline bool Place(Geometry geometry, std::span<const Mat4> rows, std::string &error) {
  if (geometry.parts() != 1 || rows.empty()) {
    error = "near preview requires one surface and actual instances";
    return false;
  }
  const auto positions = geometry.positionsOf(0);
  const auto normals = geometry.normalsOf(0);
  const auto uv = geometry.textureOf(0);
  std::vector<StoredVertex> vertices(positions.size() / 3);
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] =
        StoredVertex::Of({{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}},
                         {{uv[at * 2], uv[at * 2 + 1]}},
                         {{normals[at * 3], normals[at * 3 + 1], normals[at * 3 + 2]}});
  }
  Render::PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = geometry.trianglesOf(0);
  piece.Tangents = geometry.tangentsOf(0);
  piece.Instances = rows;
  piece.MaxInstances = static_cast<uint32_t>(rows.size());
  piece.Textured = true;
  const size_t refs = piece.Indices.size() / 3 * rows.size();
  auto material = Held.Renderer->RegisterPieceMaterials(std::move(geometry));
  if (!material) {
    error = std::move(material.error());
    return false;
  }
  piece.Surface = Render::PieceSurface::Registered(*material);
  auto placed = Held.Renderer->PlacePiece(piece);
  if (!placed) {
    error = std::move(placed.error());
    return false;
  }
  Held.Pieces.push_back(*placed);
  Held.References += refs;
  Held.Vertices += vertices.size();
  Held.ModelBytes += rows.size() * sizeof(Mat4);
  return true;
}

inline bool BuildMesh(const Generators::TreeSpecies &species,
                      std::span<const Mat4> roots,
                      size_t rank,
                      std::string &error) {
  const auto prototype = Generators::TreePrototype::Grow(species);
  if (!prototype) {
    error = "near preview source failed";
    return false;
  }
  constexpr size_t kMatrixBytesMost = size_t{128} * 1024 * 1024;
  const size_t rowsMost = (kMatrixBytesMost - Held.ModelBytes) / sizeof(Mat4);
  if (rank >= prototype->Ranks().size() ||
      prototype->Ranks()[rank].CardCount + size_t{1} > rowsMost / roots.size()) {
    error = "near experiment exceeds its explicit128MiB matrix budget";
    return false;
  }
  auto source = prototype->InstancedGeometryAt(rank);
  if (!source) {
    error = "near preview rank failed";
    return false;
  }
  if (!Place(std::move(source->Bark), roots, error)) { return false; }
  if (!source->Placements.empty()) {
    std::vector<Mat4> leaves;
    leaves.reserve(roots.size() * source->Placements.size());
    for (const auto &root : roots) {
      for (const auto &local : source->Placements) { leaves.push_back(root * local); }
    }
    if (!Place(std::move(source->Leaf), leaves, error)) { return false; }
  }
  Held.Trees += roots.size();
  return true;
}

inline bool BuildCards(std::string_view provenance,
                       std::span<const Mat4> roots,
                       size_t rank,
                       std::string &error) {
  auto cache = AssetCache::Open((std::filesystem::path(Directory) / "prototypes.sqlite").string());
  if (!cache) {
    error = "depth preview cache missing";
    return false;
  }
  auto bytes = (*cache)->Load(Sha256Hex(provenance), size_t{64} * 1024 * 1024);
  if (!bytes || !*bytes) {
    error = "depth preview atlas absent";
    return false;
  }
  auto atlas = Content::ImpostorAtlas::Decode((**bytes).Bytes(), provenance, error);
  if (!atlas) { return false; }
  Content::ImpostorCards cards;
  cards.Centre = atlas->CentreM();
  cards.HalfExtentM = atlas->HalfExtentM();
  for (size_t view = 0; view < atlas->Views().size(); ++view) {
    auto surface = Render::BuildImpostorSurface(*atlas,
                                                view,
                                                rank == 0 ? Render::ImpostorSurfaceDetail::Flat
                                                          : Render::ImpostorSurfaceDetail::Depth);
    if (!surface) {
      error = "depth preview surface failed";
      return false;
    }
    Held.Vertices += surface->positionsOf(0).size() / 3;
    Held.References += surface->trianglesOf(0).size() / 3 * roots.size();
    cards.Views.push_back(
        {.Direction = atlas->Views()[view].TowardEye, .Surface = std::move(*surface)});
  }
  auto pieces = Render::ImpostorInstances::Create(
      *Held.Renderer, cards, static_cast<uint32_t>(roots.size()), error);
  if (!pieces) { return false; }
  Held.Roots.emplace_back(roots.begin(), roots.end());
  Held.Depth.push_back(std::move(pieces));
  Held.Trees += roots.size();
  Held.ModelBytes += roots.size() * sizeof(Mat4);
  return true;
}

template <class Groups> void Dump(const Groups &groups, const Vec3 &eye) {
  static bool dumped = false;
  if (dumped) { return; }
  dumped = true;
  std::ofstream out(DumpPath);
  out << std::setprecision(std::numeric_limits<double>::max_digits10) << "{\"eye\":[" << eye[0]
      << ',' << eye[1] << ',' << eye[2] << "],\"groups\":[";
  bool firstGroup = true;
  for (const auto &group : groups) {
    if (!firstGroup) { out << ','; }
    firstGroup = false;
    out << "{\"height_m\":" << group.Species->HeightM()
        << ",\"species\":" << group.Species->Definition() << ",\"models\":[";
    bool firstModel = true;
    for (const auto &model : group.Models) {
      if (!firstModel) { out << ','; }
      firstModel = false;
      out << '[';
      for (size_t at = 0; at < model.Column.size(); ++at) {
        if (at) { out << ','; }
        out << model.Column[at];
      }
      out << ']';
    }
    out << "]}";
  }
  out << "]}";
}

struct Selection {
  size_t Mode = 0, Rank = 0;
  std::vector<Choice> Chosen;
};

template <class Groups>
std::expected<Selection, std::string_view> ReadSelection(const Groups &groups) {
  std::ifstream input(SelectionPath);
  size_t count = 0;
  Selection result;
  constexpr size_t kMostTrees = 16;
  if (!(input >> count >> result.Mode >> result.Rank) || count == 0 || count > kMostTrees ||
      result.Mode > 2 || result.Rank > 3) {
    return std::unexpected("tree detail experiment selection missing");
  }
  result.Chosen.resize(count);
  for (auto &choice : result.Chosen) {
    if (!(input >> choice.Group >> choice.Row) || choice.Group >= groups.size() ||
        choice.Row >= groups[choice.Group].Models.size()) {
      return std::unexpected("tree detail experiment source index changed");
    }
    for (auto &value : choice.Original.Column) {
      if (!(input >> value)) { return std::unexpected("tree detail experiment matrix missing"); }
    }
    if (choice.Original.Column != groups[choice.Group].Models[choice.Row].Column) {
      return std::unexpected("tree detail experiment original world model changed");
    }
  }
  return result;
}

template <class Groups>
bool Prepare(Groups &groups, Render::SceneRenderer &renderer, std::string &error) {
  const auto began = std::chrono::steady_clock::now();
  auto selected = ReadSelection(groups);
  if (!selected) {
    error = selected.error();
    return false;
  }
  const auto mode = selected->Mode;
  const auto rank = selected->Rank;
  const auto &chosen = selected->Chosen;
  Held.Renderer = &renderer;
  Held.Far.resize(groups.size());
  for (size_t group = 0; group < groups.size(); ++group) {
    std::vector<Mat4> roots;
    auto &far = Held.Far[group];
    far.reserve(groups[group].Models.size());
    for (size_t row = 0; row < groups[group].Models.size(); ++row) {
      const bool inNear = std::ranges::any_of(
          chosen, [&](const auto &choice) { return choice.Group == group && choice.Row == row; });
      (inNear ? roots : far).push_back(groups[group].Models[row]);
    }
    if (!roots.empty()) {
      const bool built =
          mode == 0 ? BuildMesh(*groups[group].Species, roots, rank, error)
                    : BuildCards(groups[group].Provenance, roots, mode == 1 ? 0 : 1, error);
      if (!built) { return false; }
    }
    groups[group].ViewOrigin.reset();
  }
  std::println(
      stderr,
      "DETAIL mode={} rank={} trees={} reference_kind={} references={} prototype_vertices={} "
      "model_bytes={} prepare_ms={:.3f}",
      mode,
      rank,
      Held.Trees,
      mode == 0 ? "model_refs" : "all_view_model_refs",
      Held.References,
      Held.Vertices,
      Held.ModelBytes,
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return true;
}

inline bool Update(const Vec3 &eye, std::string &error) {
  for (size_t at = 0; at < Held.Depth.size(); ++at) {
    if (!Held.Depth[at]->Update(Held.Roots[at], eye, error)) { return false; }
  }
  return true;
}
}
#endif
