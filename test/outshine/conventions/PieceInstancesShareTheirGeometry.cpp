#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>
#include <SDL3/SDL.h>
#include "Live.h"
#include "Subject.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) { Unprepared(SDL_GetError()); return Report(); }
  Geometry base;
  const int part = base.addPart("offscreen", base.addSurface("white", Material{}));
  CHECK(base.setPositions(part, std::array<float,18>{-100.4f,-0.4f,0,-99.6f,-0.4f,0,-100,0.4f,0,99.6f,-0.4f,0,100.4f,-0.4f,0,100,0.4f,0}), "base positions");
  CHECK(base.setNormals(part, std::array<float,18>{0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1}), "base normals");
  CHECK(base.setTriangles(part, std::array<uint32_t,6>{0,1,2,3,4,5}), "base indices");
  Material masked;
  masked.BaseColour = {{1,1,1,1}};
  masked.Alpha = AlphaMode::Masked;
  masked.DoubleSided = true;
  masked.BaseColourMap.Image = base.addImage(2, 1, std::array<uint8_t,8>{255,255,255,0,255,255,255,255});
  masked.BaseColourMap.Sampler.Minify = masked.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  masked.BaseColourMap.Sampler.Mip = MipFilter::None;
  const int maskPart = base.addPart("masked-offscreen", base.addSurface("masked", masked));
  CHECK(base.setPositions(maskPart, std::array<float,9>{99.6f,-0.4f,0,100.4f,-0.4f,0,100,0.4f,0}), "masked base positions");
  CHECK(base.setNormals(maskPart, std::array<float,9>{0,0,1,0,0,1,0,0,1}), "masked base normals");
  CHECK(base.setTexture(maskPart, std::array<float,6>{0,0,1,0,0,1}), "masked base UVs");
  CHECK(base.setTriangles(maskPart, std::array<uint32_t,3>{0,1,2}), "masked base indices");
  Gltf::Subject built;
  CHECK(built.Assemble(base), "base subject assembles");
  Render::SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 320;
  declaration.Built = &built;
  declaration.Outputs = {"sceneLinear", "sceneDepth"};
  declaration.KeyLux = 20000;
  declaration.KeyElevationDeg = 45;
  declaration.KeyBearingDeg = 180;
  std::unique_ptr<Core::Live> scene;
  std::string error;
  if (!Core::Live::Open(renderer, declaration, nullptr, scene, error)) {
    Unprepared(error.c_str()); return Report();
  }
  Render::Viewpoint eye;
  eye.EyeM = {{0,0,4}};
  eye.Kind = Render::CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = 0.1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  const std::array<StoredVertex,3> vertices{{
      StoredVertex::Of({{-0.4f,-0.4f,0}},{{0,0}},{{0,0,1}}),
      StoredVertex::Of({{0.4f,-0.4f,0}},{{0,0}},{{0,0,1}}),
      StoredVertex::Of({{0,0.4f,0}},{{0,0}},{{0,0,1}})}};
  const std::array<uint32_t,3> indices{0,1,2};
  std::array<Mat4,3> rows{};
  rows[0][12] = 100;
  rows[1][12] = -1;
  rows[2][12] = 1;
  const std::array<uint32_t,2> surfaces{0,1};
  renderer.WearPieces(surfaces);
  std::filesystem::create_directories("build/instance-native");
  for (const bool clustered : {false,true}) {
    const std::array<DagCluster,1> clusters{{{.SelfRadius=0.6f,.ParentRadius=0.6f,
                                           .ParentErr=kDagRootErr,.Count=3}}};
    Render::PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Instances = rows;
    if (clustered) { piece.Clusters = clusters; }
    const auto id = renderer.PlacePiece(piece, error);
    CHECK(id != Render::kNoPiece, "three placements accept one prototype");
    CHECK(renderer.PiecesStanding() == 1 && renderer.PieceTriangles() == 1,
          "resident triangle count does not multiply with placements");
    Render::PieceMesh neighbour;
    neighbour.Verts = vertices;
    neighbour.Indices = indices;
    neighbour.Row[13] = 1;
    const auto neighbourId = renderer.PlacePiece(neighbour, error);
    CHECK(neighbourId != Render::kNoPiece, "a following single-placement mesh accepts its own row");
    CHECK(scene->Draw(error), "instanced frame draws");
    renderer.WaitForGpu();
    std::vector<float> depth;
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready, "depth is read");
    CHECK(depth.size() == 320u*320u, "complete depth frame");
    if (depth.size() == 320u*320u) {
      CHECK(depth[160u*320u+80u] > 0 && depth[160u*320u+240u] > 0,
            "both visible instances survive even when the first is outside the frustum");
      CHECK(depth[160u*320u+160u] == 0, "the gap contains no unplaced prototype");
    }
    CHECK(scene->Screenshot(clustered ? "build/instance-native/clustered.png" : "build/instance-native/instanced.png", error), "instanced PNG is written");
    renderer.ReleasePiece(id);
    CHECK(renderer.PiecesStanding() == 1 && renderer.PieceTriangles() == 1, "release retains only the other prototype");
    CHECK(scene->Draw(error), "released frame draws");
    renderer.WaitForGpu();
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready, "released depth is read");
    CHECK(depth.size() == 320u*320u && depth[160u*320u+80u] == 0 && depth[160u*320u+240u] == 0,
          "release removes every instance");
    CHECK(depth.size() == 320u*320u && depth[80u*320u+160u] > 0,
          "the following piece keeps its matrix after earlier rows are released");
    renderer.ReleasePiece(neighbourId);
    CHECK(renderer.PiecesStanding() == 0 && renderer.PieceTriangles() == 0,
          "both allocations are released without counter underflow");

  }
  const std::array<StoredVertex,4> card{{
      StoredVertex::Of({{-0.4f,-0.4f,0}},{{0,1}},{{0,0,1}}),
      StoredVertex::Of({{0.4f,-0.4f,0}},{{1,1}},{{0,0,1}}),
      StoredVertex::Of({{0.4f,0.4f,0}},{{1,0}},{{0,0,1}}),
      StoredVertex::Of({{-0.4f,0.4f,0}},{{0,0}},{{0,0,1}})}};
  const std::array<uint32_t,6> cardIndices{0,1,2,0,2,3};
  const std::array<DagCluster,1> cardClusters{{{.SelfRadius=0.6f,.ParentRadius=0.6f,
                                             .ParentErr=kDagRootErr,.Count=6}}};
  for (const bool clustered : {false,true}) {
    for (const bool back : {false,true}) {
      std::array<Mat4,2> placements{rows[1], rows[2]};
      if (back) {
        for (auto &placement : placements) { placement[0] = placement[10] = -1; }
      }
      Render::PieceMesh piece;
      piece.Verts = card;
      piece.Indices = cardIndices;
      piece.Instances = placements;
      piece.Surface = 1;
      piece.Textured = true;
      if (clustered) { piece.Clusters = cardClusters; }
      const auto id = renderer.PlacePiece(piece, error);
      CHECK(id != Render::kNoPiece && scene->Draw(error), "masked instanced cards draw");
      renderer.WaitForGpu();
      std::vector<float> depth;
      CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready && depth.size() == 320u*320u,
            "masked card depth is complete");
      if (depth.size() == 320u*320u) {
        for (const size_t centre : {80u,240u}) {
          const size_t opaque = back ? centre-20u : centre+20u;
          const size_t clear = back ? centre+20u : centre-20u;
          CHECK(depth[160u*320u+opaque] > 0, "the opaque half survives on either side of each instance");
          CHECK(depth[160u*320u+clear] == 0, "the masked half writes no depth on either side of each instance");
        }
      }
      const std::string path = std::string("build/instance-native/masked-") +
          (clustered ? "clustered-" : "direct-") + (back ? "back.png" : "front.png");
      CHECK(scene->Screenshot(path, error), "masked card PNG is written");
      renderer.ReleasePiece(id);
    }
  }
  return Report();
}
