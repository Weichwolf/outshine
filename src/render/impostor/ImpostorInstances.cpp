#include "ImpostorInstances.h"

#include "ImpostorSurface.h"
#include "SceneRenderer.h"
#include "StoredVertex.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace Says {
constexpr auto Capacity = "impostor instances require views and a positive instance capacity";
constexpr auto View = "impostor view has no native card geometry";
constexpr auto Limit = "impostor instance count exceeds its declared capacity";
}

std::unique_ptr<ImpostorInstances> ImpostorInstances::Create(SceneRenderer &renderer,
                                                             const Content::ImpostorAtlas &atlas,
                                                             uint32_t maxInstances,
                                                             std::string &error) {
  auto cards = PrepareImpostorCards(atlas, error);
  return cards ? Create(renderer, *cards, maxInstances, error) : nullptr;
}

std::unique_ptr<ImpostorInstances> ImpostorInstances::Create(SceneRenderer &renderer,
                                                             const Content::ImpostorCards &cards,
                                                             uint32_t maxInstances,
                                                             std::string &error) {
  if (maxInstances == 0 || cards.Views.empty()) {
    error = Says::Capacity;
    return nullptr;
  }
  auto result = std::unique_ptr<ImpostorInstances>(
      new ImpostorInstances(renderer, cards.Centre, maxInstances));
  result->Views_.reserve(cards.Views.size());
  for (const auto &view : cards.Views) {
    if (view.Surface.parts() != 1) {
      error = Says::View;
      return nullptr;
    }
    auto geometry = view.Surface.clone();
    const auto positions = geometry.positionsOf(0);
    const auto normals = geometry.normalsOf(0);
    const auto uv = geometry.textureOf(0);
    if (positions.empty() || normals.size() != positions.size() ||
        uv.size() != positions.size() / 3 * 2) {
      error = Says::View;
      return nullptr;
    }
    std::vector<StoredVertex> vertices(positions.size() / 3);
    for (size_t at = 0; at < vertices.size(); ++at) {
      vertices[at] =
          StoredVertex::Of({{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}},
                           {{uv[at * 2], uv[at * 2 + 1]}},
                           {{normals[at * 3], normals[at * 3 + 1], normals[at * 3 + 2]}});
    }
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = geometry.trianglesOf(0);
    piece.Tangents = geometry.tangentsOf(0);
    piece.Textured = true;
    piece.MaxInstances = maxInstances;
    auto material = renderer.RegisterPieceMaterials(std::move(geometry));
    if (!material) {
      error = std::move(material).error();
      return nullptr;
    }
    piece.Surface = PieceSurface::Registered(*material);
    View held;
    held.Direction = view.Direction;
    held.NextRows.reserve(maxInstances);
    auto placed = renderer.PlacePiece(piece);
    if (!placed) {
      error = std::move(placed.error());
      return nullptr;
    }
    held.Piece = *placed;
    result->Views_.push_back(std::move(held));
    if (!renderer.SetPieceInstances(result->Views_.back().Piece, {}, error)) { return nullptr; }
  }
  return result;
}

ImpostorInstances::~ImpostorInstances() {
  for (const auto &view : Views_) { Renderer_->ReleasePiece(view.Piece); }
}

bool ImpostorInstances::Update(std::span<const Mat4> models, const Vec3 &eye, std::string &error) {
  if (models.size() > MaxInstances_) {
    error = Says::Limit;
    return false;
  }
  for (auto &view : Views_) { view.NextRows.clear(); }
  for (const auto &model : models) {
    const Vec3 toward = eye - model.TransformPoint(Centre_);
    size_t selected = 0;
    double best = -std::numeric_limits<double>::infinity();
    for (size_t at = 0; at < Views_.size(); ++at) {
      const double score = Dot(toward, model.TransformDirection(Views_[at].Direction));
      if (score > best) {
        best = score;
        selected = at;
      }
    }
    Views_[selected].NextRows.push_back(model);
  }
  std::vector<SceneResources::PieceRows> changes;
  changes.reserve(Views_.size());
  for (const auto &view : Views_) {
    changes.push_back({.Piece = view.Piece, .Rows = view.NextRows});
  }
  return Renderer_->SetPieceInstances(changes, error);
}

}
