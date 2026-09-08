#include <array>
#include <cstdio>
#include <cmath>
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
  masked.NormalMap.Image = base.addImage(1,1,std::array<uint8_t,4>{191,159,231,255});
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
  declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  declaration.KeyLux = 20000;
  declaration.Exposure = 1.0;
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
  const std::array<float,16> tangents{1,0,0,-1,1,0,0,-1,1,0,0,-1,1,0,0,-1};
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
      piece.Tangents = tangents;
      piece.Indices = cardIndices;
      piece.Instances = placements;
      piece.Surface = 1;
      piece.Textured = true;
      if (clustered) { piece.Clusters = cardClusters; }
      auto invalid = piece;
      invalid.Tangents = std::span<const float>(tangents).first(12);
      CHECK(renderer.PlacePiece(invalid,error) == Render::kNoPiece, "incomplete tangent streams are refused");
      invalid = piece;
      invalid.Textured = false;
      CHECK(renderer.PlacePiece(invalid,error) == Render::kNoPiece, "tangents without UVs are refused");
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
      std::vector<float> normals;
      CHECK(renderer.ReadShadingNormal(normals) == Render::ReadState::Ready && normals.size()==320u*320u*4u,
            "instanced shading normals are readable");
      Vec3 expected{{191.0/127.5-1, -(159.0/127.5-1), 231.0/127.5-1}};
      if (back) { expected[1] = -expected[1]; }
      CHECK(Normalise(expected), "the external normal-map direction is nonzero");
      if (normals.size()==320u*320u*4u) {
        for (const size_t centre : {80u,240u}) {
          const size_t opaque = back ? centre-20u : centre+20u;
          const size_t pixel=(160u*320u+opaque)*4u;
          const Vec3 actual{{normals[pixel],normals[pixel+1],normals[pixel+2]}};
          const auto delta=actual-expected;
          CHECK(std::sqrt(Dot(delta,delta)) < std::sqrt(3.0)/1024,
                "rotated double-sided instances preserve the normal-map world direction");
        }
      }
      const std::string path = std::string("build/instance-native/masked-") +
          (clustered ? "clustered-" : "direct-") + (back ? "back.png" : "front.png");
      CHECK(scene->Screenshot(path, error), "masked card PNG is written");
      renderer.ReleasePiece(id);
    }
  }
  Render::PieceMesh resident;
  resident.Verts = vertices;
  resident.Indices = indices;
  resident.Row = rows[1];
  const auto oldPiece = renderer.PlacePiece(resident, error);
  CHECK(oldPiece != Render::kNoPiece && scene->Draw(error), "resident prototype stands before material arrival");
  renderer.WaitForGpu();
  std::vector<float> before, after;
  CHECK(renderer.ReadSceneLinear(before) == Render::ReadState::Ready && before.size()==320u*320u*4u,
        "resident colour is captured before arrival");
  CHECK(scene->Screenshot("build/instance-native/material-before.png", error), "before-arrival PNG is written");
  std::array<Render::SubjectMaterial,2> rejected;
  rejected[1].Row.Transmission = 1;
  CHECK(!renderer.AppendSubjectMaterials(rejected, error), "unsupported second material refuses the complete append");
  error.clear();
  const std::array<uint8_t,4> green{32,180,64,255};
  Render::SubjectMaterial arriving;
  arriving.Row.Unlit = true;
  arriving.Row.BaseColour = {{0.5f,1.0f,0.5f,1.0f}};
  arriving.Colour.Rgba = green.data();
  arriving.Colour.Width = arriving.Colour.Height = 1;
  CHECK(renderer.AppendSubjectMaterials(std::span(&arriving,1), error), "a texture material appends to the resident table");
  CHECK(scene->Draw(error), "resident draws survive material arrival");
  renderer.WaitForGpu();
  CHECK(renderer.ReadSceneLinear(after) == Render::ReadState::Ready && after==before,
        "appending a surface preserves every resident pixel");
  const std::array<uint32_t,3> extendedSurfaces{0,1,2};
  renderer.WearPieces(extendedSurfaces);
  resident.Row = rows[2];
  resident.Surface = 2;
  resident.Textured = true;
  const auto newPiece = renderer.PlacePiece(resident,error);
  CHECK(newPiece != Render::kNoPiece && scene->Draw(error), "a new prototype draws with the appended surface");
  renderer.WaitForGpu();
  CHECK(renderer.ReadSceneLinear(after) == Render::ReadState::Ready && after.size()==before.size(),
        "both resident and arriving prototype colours are readable");
  if (after.size()==320u*320u*4u && before.size()==after.size()) {
    const size_t oldPixel=(160u*320u+80u)*4u, newPixel=(160u*320u+240u)*4u;
    CHECK(std::equal(before.begin()+oldPixel,before.begin()+oldPixel+4,after.begin()+oldPixel),
          "old material slot remains unchanged after new geometry arrives");
    CHECK(after[newPixel+1] > 0.4f && after[newPixel] < 0.1f && after[newPixel+2] < 0.1f,
          "the first appended slot samples its green texture without a partial rejected batch");
    for (size_t channel=0; channel<3; ++channel) {
      const double encoded=green[channel]/255.0;
      const double linear=encoded<=0.04045 ? encoded/12.92 : std::pow((encoded+0.055)/1.055,2.4);
      CHECK_NEAR(after[newPixel+channel],linear*arriving.Row.BaseColour[channel],1.0/4096,"linear",
                 "unlit instance retains the decoded texture value within half-float rounding");
    }
  }
  CHECK(scene->Screenshot("build/instance-native/material-after.png", error), "after-arrival PNG is written");
  renderer.ReleasePiece(newPiece);
  renderer.ReleasePiece(oldPiece);
  CHECK(scene->Restand(built,0,error), "Live rebuilds after the direct renderer registration fixture");
  scene->Eye(eye);
  Geometry empty;
  CHECK(!scene->RegisterPieceSurfaces(std::move(empty),error), "registration refuses an empty material source");
  Geometry missing;
  Material absent;
  absent.BaseColourMap.Image = 0;
  (void)missing.addSurface("missing image",absent);
  CHECK(!scene->RegisterPieceSurfaces(std::move(missing),error), "registration refuses a missing native image");
  Geometry glass;
  Material transmitted;
  transmitted.Transmission = 1;
  (void)glass.addSurface("undeclared glass pass",transmitted);
  CHECK(!scene->RegisterPieceSurfaces(std::move(glass),error), "registration refuses an unsupported pass");
  error.clear();
  const auto registerColour = [&](const std::array<uint8_t,4> &colour) {
    Geometry source;
    Material material;
    material.BaseColour = {{1,1,1,1}};
    material.Unlit = true;
    material.BaseColourMap.Image = source.addImage(1,1,colour);
    (void)source.addSurface("registered prototype",material);
    return scene->RegisterPieceSurfaces(std::move(source),error);
  };
  const auto registeredGreen = registerColour(green);
  CHECK(registeredGreen && *registeredGreen==0, "refused registrations consume no stable material handles");
  if (!registeredGreen) { return Report(); }
  Render::PieceMesh owned;
  owned.Verts = vertices;
  owned.Indices = indices;
  owned.Instances = std::span(rows).subspan(1);
  owned.Surface = Render::PieceSurface::Registered(*registeredGreen);
  owned.Textured = true;
  const auto ownedPiece = scene->PlacePiece(owned,error);
  CHECK(ownedPiece != Render::kNoPiece && scene->Draw(error), "registered texture reaches two instances after source destruction");
  renderer.WaitForGpu();
  std::vector<float> registeredBefore, registeredAfter;
  CHECK(renderer.ReadSceneLinear(registeredBefore)==Render::ReadState::Ready && registeredBefore.size()==320u*320u*4u,
        "registered instances have a complete linear image");
  if (registeredBefore.size()==320u*320u*4u) {
    for (const size_t x : {80u,240u}) {
      for (size_t channel=0; channel<3; ++channel) {
        const double encoded=green[channel]/255.0;
        const double linear=encoded<=0.04045 ? encoded/12.92 : std::pow((encoded+0.055)/1.055,2.4);
        CHECK_NEAR(registeredBefore[(160u*320u+x)*4u+channel],linear,1.0/4096,"linear",
                   "Live-owned prototype images preserve decoded RGB in each instance");
      }
    }
  }
  const auto registeredBlue = registerColour({32,64,180,255});
  CHECK(registeredBlue && *registeredBlue==*registeredGreen+1, "another prototype receives a distinct stable handle");
  CHECK(scene->Draw(error), "new registration preserves the existing instances");
  renderer.WaitForGpu();
  CHECK(renderer.ReadSceneLinear(registeredAfter)==Render::ReadState::Ready && registeredAfter==registeredBefore,
        "another image owner does not alter resident pixels");
  CHECK(scene->Screenshot("build/instance-native/registered-before.png",error), "registered before-rebuild PNG is written");
  (void)base.addSurface("extra native material one",Material{});
  (void)base.addSurface("extra native material two",Material{});
  Gltf::Subject rebuilt;
  CHECK(rebuilt.Assemble(base) && scene->Restand(rebuilt,0,error), "native material growth rebuilds around resident registered pieces");
  scene->Eye(eye);
  CHECK(scene->Draw(error), "registered instances draw after native material indices shift");
  renderer.WaitForGpu();
  CHECK(renderer.ReadSceneLinear(registeredAfter)==Render::ReadState::Ready && registeredAfter==registeredBefore,
        "rebuilding the native surface table preserves every registered pixel without replacing pieces");
  CHECK(scene->Screenshot("build/instance-native/registered-after.png",error), "registered after-rebuild PNG is written");
  if (registeredBlue) {
    owned.Instances = {};
    owned.Row[13] = 1;
    owned.Surface = Render::PieceSurface::Registered(*registeredBlue);
    const auto bluePiece = scene->PlacePiece(owned,error);
    CHECK(bluePiece!=Render::kNoPiece && scene->Draw(error), "a retained second handle remains usable after rebuild");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(registeredAfter)==Render::ReadState::Ready && registeredAfter.size()==320u*320u*4u,
          "second retained prototype has readable pixels");
    if (registeredAfter.size()==320u*320u*4u) {
      const size_t pixel=(80u*320u+160u)*4u;
      CHECK(registeredAfter[pixel+2]>0.4f && registeredAfter[pixel]<0.1f && registeredAfter[pixel+1]<0.1f,
            "the second handle still samples its blue image");
    }
    CHECK(scene->Screenshot("build/instance-native/registered-both.png",error), "both retained prototype PNG is written");
    scene->ReleasePiece(bluePiece);
  }
  scene->ReleasePiece(ownedPiece);
  return Report();
}
