#include "GroundWorldCandidate.h"

#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace {
struct BuiltGeometry {
  outshine::Geometry Mesh;
  outshine::Material Surface;
  outshine::MaterialInstance SurfaceInstance;
};

BuiltGeometry Triangle(std::string name, float eastM, std::array<float, 3> colour) {
  BuiltGeometry built;
  built.Surface.BaseColour = {colour[0], colour[1], colour[2], 1.0f};
  const auto surface = built.Mesh.addSurface(name + " surface", built.Surface);
  const auto part = built.Mesh.addPart(name, surface.value_or(outshine::MaterialInstance{}));
  if (surface && part) {
    const std::array<float, 9> positions{eastM - 1, -1, 0, eastM + 1, -1, 0, eastM, 1, 0};
    (void)built.Mesh.setPositions(*part, positions);
    (void)built.Mesh.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2});
    (void)built.Mesh.addLamp(name + " lamp", outshine::PunctualLight{}, outshine::Mat4{});
    built.SurfaceInstance = *surface;
  }
  return built;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    BuiltGeometry authored = Triangle("authored", -4, {1, 0, 0});
    Core::Declaration declaration;
    declaration.InitialGeometry = &authored.Mesh;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "authored subject opens");
    CHECK(scene && scene->DrivenParts() == 1, "the initial authored part is driven");

    Surrounds world;
    Ground::BuildingField footprints;
    world.BindSceneResources(renderer);
    for (const auto [revision, eastM] :
         std::array<std::pair<uint64_t, float>, 2>{std::pair{1u, 2.0f}, std::pair{2u, 6.0f}}) {
      if (!scene) { break; }
      GroundWorldCandidate candidate(renderer, world, footprints);
      const auto prepared = candidate.Prepare(*scene, nullptr);
      CHECK(prepared.has_value(),
            prepared ? "ground candidate prepares" : prepared.error().c_str());
      if (!prepared) { continue; }
      BuiltGeometry generated = Triangle("generated", eastM, {0, 1, 0});
      auto began = candidate.BeginGroundGeometryBuild(
          std::move(generated.Mesh), generated.SurfaceInstance, generated.Surface);
      CHECK(began.has_value(), began ? "generated geometry build begins" : began.error().c_str());
      std::expected<bool, std::string> advanced = false;
      if (began) {
        advanced = candidate.AdvanceGroundGeometryBuild(std::numeric_limits<size_t>::max());
      }
      CHECK(advanced && *advanced, "generated geometry build completes");
      CHECK(candidate.Publish(world, footprints, scene, GroundRevision{.Region = revision})
                .has_value(),
            "ground candidate publishes");
      if (!scene) { continue; }
      const Render::Shape &shown = scene->Shown();
      CHECK(
          scene->DrivenParts() == 1 && shown.Parts.size() == 2 && shown.Surfaces.size() == 2 &&
              shown.Lamps.size() == 2 && shown.Parts[0].Name == "authored" &&
              shown.Parts[1].Name == "generated" && shown.Parts[0].PositionsM[0] == -5 &&
              shown.Parts[1].PositionsM[0] == eastM - 1,
          "each publication preserves authored geometry and replaces obsolete generated geometry");
    }
  }
  SDL_Quit();
  return Report();
}
