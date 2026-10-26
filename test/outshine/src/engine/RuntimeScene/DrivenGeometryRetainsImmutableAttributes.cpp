#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

int CheckSource() {
  Geometry source;
  const auto part = source.addPart("asset", MaterialInstance{});
  CHECK(part.has_value(), "the source part exists");
  if (!part) { return Report(); }
  constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
  CHECK(source.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            source.setNormals(*part, normals) &&
            source.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the source has complete authored attributes");
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear"};
  declaration.Materials.front().Unlit = true;
  declaration.InitialGeometry = &source;
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::unique_ptr<Core::RuntimeScene> candidate;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return Report(); }
  const auto attributes = scene->Shown().Parts.front().Normals;
  CHECK(std::ranges::equal(attributes, normals), "the owned source supplies the authored normals");
  CHECK(attributes.data() != source.normalsOf(*part).data(),
        "borrowed caller geometry is isolated at registration");
  CHECK(source.setNormals(*part, std::array<float, 9>{0, 1, 0, 0, 1, 0, 0, 1, 0}) &&
            std::ranges::equal(attributes, normals),
        "later caller edits cannot mutate the immutable scene source");

  const auto prepare = [&] {
    return Core::RuntimeScene::PreparesWorldReplacement(renderer,
                                                        *scene,
                                                        nullptr,
                                                        candidate,
                                                        error,
                                                        Render::SceneResources::PieceSources::Copy,
                                                        Core::SubjectGeometrySources::Driven);
  };
  CHECK(prepare(), error.c_str());
  if (!candidate) { return Report(); }
  CHECK(candidate->Shown().Parts.front().Normals.data() == attributes.data(),
        "an unchanged driven source crosses world preparation without an attribute copy");
  candidate.reset();
  renderer.AbandonsWorldCandidate();
  CHECK(std::ranges::equal(attributes, normals),
        "abandoning the candidate leaves the published source alive");
  CHECK(prepare(), error.c_str());
  if (!candidate) { return Report(); }
  CHECK(Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
        error.c_str());
  CHECK(scene->Shown().Parts.front().Normals.data() == attributes.data() &&
            std::ranges::equal(attributes, normals),
        "publication keeps the same source after destroying the old scene");

  Geometry combined = source.clone();
  CHECK(combined.setNormals(*part, normals) && combined.append(source),
        "a mixed replacement combines the driven and independent generated attributes");
  CHECK(Core::RuntimeScene::PreparesGeometryReplacement(
            renderer, *scene, std::move(combined), 1, nullptr, candidate, error),
        error.c_str());
  if (!candidate) { return Report(); }
  CHECK(candidate->Shown().Parts.size() == 2 &&
            candidate->Shown().Parts.front().Normals.data() != attributes.data() &&
            std::ranges::equal(candidate->Shown().Parts.front().Normals, normals),
        "real composition owns independent attributes without mutating the retained source");
  CHECK(Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
        error.c_str());
  CHECK(prepare(), error.c_str());
  if (!candidate) { return Report(); }
  CHECK(candidate->Shown().Parts.size() == 1 &&
            candidate->Shown().Parts.front().Normals.data() == attributes.data() &&
            std::ranges::equal(candidate->Shown().Parts.front().Normals, normals),
        "a mixed world retains the original driven lease for subsequent reconfiguration");
  candidate.reset();
  renderer.AbandonsWorldCandidate();
  return Report();
}
}

int main() {
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    CHECK(false, SDL_GetError());
    return Report();
  }
  const int result = CheckSource();
  SDL_Quit();
  return result;
}
