#include "SubjectDraw.h"
#include "SceneResources.h"
#include "Check.h"

#include <array>
#include <limits>
#include <span>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  const std::array<StoredVertex, 3> vertices{};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  std::array<float, 6> coordinates{0, 0, 3, 0, 3, 2};
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  CHECK(ValidPieceUv1(piece),
        "an absent optional UV1 stream preserves the existing piece contract");
  piece.Textured = true;
  piece.Uv1 = coordinates;
  CHECK(ValidPieceUv1(piece),
        "physical metre values outside normalized texture range remain valid");
  SubjectDraw subjects;
  SceneResources resources;
  std::string error;
  const auto reject = [&] {
    CHECK(!ValidPieceUv1(piece), "inconsistent second UV input is invalid");
    CHECK(subjects.PlacePiece(piece, error) == kNoPiece && error == kPieceUv1Error,
          "the actual subject entry rejects malformed data before device access");
    const auto result = resources.PlacePiece(subjects, piece);
    CHECK(!result && result.error() == kPieceUv1Error,
          "the scene resource entry rejects malformed data before taking an ownership copy");
    CHECK(resources.PieceSourceCount() == 0 && subjects.PiecesStanding() == 0,
          "failed UV1 admission leaves no owned source or resident piece");
  };
  piece.Uv1 = std::span(coordinates).first(5);
  reject();
  piece.Uv1 = coordinates;
  piece.Textured = false;
  reject();
  piece.Textured = true;
  coordinates[0] = std::numeric_limits<float>::infinity();
  reject();
  coordinates[0] = std::numeric_limits<float>::quiet_NaN();
  reject();
  coordinates[0] = 0;
  CHECK(ValidPieceUv1(piece), "the complete finite source is admissible again");
  return Report();
}
