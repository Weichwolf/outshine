#ifndef GLTFSTUDIO_H
#define GLTFSTUDIO_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "DrawList.h"
#include "Subject.h"
#include "SubjectDraw.h"

namespace outshine::Render {
class Renderer;
}

namespace outshine::Clients {

constexpr double kStudioAnchorEcefM[3] = {6378137.0, 0.0, 0.0};

void EcefFromGltf(const double gltf[3], double out[3]);

struct Studio {
  const Gltf::Subject *Geometry = nullptr;
  Gltf::Placement Eye;

  std::vector<std::array<float, 3>> EmittedRadiance;

  std::vector<uint32_t> PartSurface;

  std::vector<std::array<double, 16>> PartPlacement;

  std::vector<Render::SubjectMaterial> Surfaces;

  const Gltf::Subject *Previous = nullptr;

  std::vector<outshine::PunctualLight> Lights;

  Render::SubjectEnvironment Environment;
};

struct StudioScratch {
  std::vector<float> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<Render::SubjectLight> Lights;
  Render::DrawList Draws;
};

[[nodiscard]] bool Aim(Render::Renderer &renderer, const Gltf::Subject &subject,
                       const Gltf::Placement &eye, std::string &error);

[[nodiscard]] bool Show(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                        std::string &error);

[[nodiscard]] bool Surface(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                           std::string &error);

[[nodiscard]] bool Place(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                        std::string &error);

[[nodiscard]] bool Move(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                        std::string &error);

}
#endif
