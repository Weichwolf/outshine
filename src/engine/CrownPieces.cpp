#include "CrownPieces.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Live.h"
#include <array>
#include <limits>

namespace outshine {
namespace Says {
constexpr auto CrownCapacity = "crown pieces require views and a positive instance capacity";
constexpr auto CrownView = "crown view has no native card geometry";
constexpr auto CrownLimit = "crown instance count exceeds its declared capacity";
}

std::unique_ptr<CrownPieces> CrownPieces::Create(Core::Live &live,
                                                 const CrownAtlas &atlas,
                                                 uint32_t maxInstances,
                                                 std::string &error) {
  if (maxInstances == 0 || atlas.Views().empty()) {
    error = Says::CrownCapacity;
    return nullptr;
  }
  auto result = std::unique_ptr<CrownPieces>(new CrownPieces(live, atlas.CentreM(), maxInstances));
  result->Views_.reserve(atlas.Views().size());
  for (size_t view = 0; view < atlas.Views().size(); ++view) {
    auto geometry = atlas.GeometryAt(view);
    if (!geometry || geometry->parts() != 1) {
      error = Says::CrownView;
      return nullptr;
    }
    const auto positions = geometry->positionsOf(0), normals = geometry->normalsOf(0);
    const auto uv = geometry->textureOf(0);
    std::vector<StoredVertex> vertices(positions.size() / 3);
    for (size_t at = 0; at < vertices.size(); ++at) {
      vertices[at] =
          StoredVertex::Of({{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}},
                           {{uv[at * 2], uv[at * 2 + 1]}},
                           {{normals[at * 3], normals[at * 3 + 1], normals[at * 3 + 2]}});
    }
    Render::PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = geometry->trianglesOf(0);
    piece.Tangents = geometry->tangentsOf(0);
    piece.Textured = true;
    piece.MaxInstances = maxInstances;
    const auto material = live.RegisterPieceSurfaces(std::move(*geometry), error);
    if (!material) { return nullptr; }
    piece.Surface = Render::PieceSurface::Registered(*material);
    View held;
    held.Direction = atlas.Views()[view].TowardEye;
    held.Rows.reserve(maxInstances);
    held.Piece = live.PlacePiece(piece, error);
    if (held.Piece == Render::kNoPiece) { return nullptr; }
    result->Views_.push_back(std::move(held));
    if (!live.SetPieceInstances(result->Views_.back().Piece, {}, error)) { return nullptr; }
  }
  return result;
}

CrownPieces::~CrownPieces() {
  for (const auto &view : Views_) { Live_->ReleasePiece(view.Piece); }
}

bool CrownPieces::Update(std::span<const Mat4> models, const Vec3 &eye, std::string &error) {
  if (models.size() > MaxInstances_) {
    error = Says::CrownLimit;
    return false;
  }
  for (auto &view : Views_) { view.Rows.clear(); }
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
    Views_[selected].Rows.push_back(model);
  }
  for (auto &view : Views_) {
    if (!Live_->SetPieceInstances(view.Piece, view.Rows, error)) { return false; }
  }
  return true;
}
}
