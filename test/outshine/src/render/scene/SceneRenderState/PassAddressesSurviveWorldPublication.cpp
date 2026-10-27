#include "SceneRenderer.h"
#include "SceneRenderState.h"
#include "GeometryRenderer.h"
#include "TerrainRenderer.h"
#include "Lens.h"
#include "Viewing.h"
#include "Check.h"
#include <string>
#include <type_traits>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

void SetNear(SceneRenderer &renderer, float nearM) {
  renderer.SetCamera(CameraBasis{},
                     Lens{.WidePx = 32, .HighPx = 32, .FovDeg = 60, .NearM = nearM, .FarM = 100});
}

void PassesKeepTheirAddresses() {
  CHECK(!std::is_move_constructible_v<SceneRenderState>,
        "scene and pass addresses remain stable behind the published ownership lease");
  CHECK(!std::is_move_assignable_v<SceneRenderState>,
        "publication transfers ownership without moving a pass or its scene residency");
  CHECK(!std::is_move_constructible_v<GeometryRenderer>, "geometry pass cannot copy self-pointers");
  CHECK(!std::is_move_assignable_v<GeometryRenderer>, "geometry pass cannot replace its address");
  CHECK(!std::is_move_constructible_v<TerrainRenderer>, "terrain pass cannot copy self-pointers");
  CHECK(!std::is_move_assignable_v<TerrainRenderer>, "terrain pass cannot replace its address");
}

void WorldsStillPublishAndAbandon() {
  SceneRenderer renderer;
  SetNear(renderer, 1);
  std::string error;
  const bool began = renderer.BeginsWorldCandidate(error);
  CHECK(began, error.c_str());
  if (!began) { return; }
  {
    const auto edit = renderer.EditWorldCandidate();
    SetNear(renderer, 2);
  }
  CHECK(renderer.NearMetres() == 1, "preparation preserves the published camera");
  const bool published = renderer.PublishesWorldCandidate(error);
  CHECK(published, error.c_str());
  if (!published) { return; }
  CHECK(!renderer.HasWorldCandidate() && renderer.NearMetres() == 2,
        "scene ownership transfer publishes the candidate camera");
  CHECK(renderer.BeginsWorldCandidate(error), "the next world can be prepared");
  {
    const auto edit = renderer.EditWorldCandidate();
    SetNear(renderer, 3);
  }
  renderer.AbandonsWorldCandidate();
  CHECK(!renderer.HasWorldCandidate() && renderer.NearMetres() == 2,
        "discarding preparation preserves the published scene");
}
}

int main() {
  PassesKeepTheirAddresses();
  WorldsStillPublishAndAbandon();
  return outshine::Test::Report();
}
