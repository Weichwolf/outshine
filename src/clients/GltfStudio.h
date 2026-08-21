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

void PlacedInEcef(const double gltf[16], double out[16]);

struct Studio {
  const Gltf::Subject *Geometry = nullptr;
  Gltf::Placement Eye;

  bool EyeStandsInside = false;

  std::vector<std::array<float, 3>> EmittedRadiance;

  std::vector<uint32_t> PartSurface;

  std::vector<std::array<double, 16>> PartPlacement;

  std::vector<Render::SubjectMaterial> Surfaces;

  const Gltf::Subject *Previous = nullptr;

  std::vector<outshine::PunctualLight> Lights;

  Render::SubjectEnvironment Environment;
};

void Placements(const Studio &studio, std::vector<double> &into);

struct StudioScratch {
  std::vector<float> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<Render::SubjectLight> Lights;
  Render::DrawList Draws;
  std::vector<double> Placements;
};

[[nodiscard]] bool Aim(Render::Renderer &renderer, const Gltf::Subject &subject,
                       const Gltf::Placement &eye, std::string &error,
                       bool standsInside = false);

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
