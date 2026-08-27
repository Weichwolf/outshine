#ifndef OUTSHINE_ENGINE_GLTFSTUDIO_H
#define OUTSHINE_ENGINE_GLTFSTUDIO_H

#include "Wgs84.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "DrawList.h"
#include "Axes.h"
#include "Subject.h"
#include "SubjectDraw.h"

namespace outshine::Render {
class Renderer;
}

namespace outshine::Clients {

constexpr double kStudioAnchorEcefM[3] = {Data::kWgs84A, 0.0, 0.0};

struct Studio {
  const Gltf::Subject *Geometry = nullptr;
  Gltf::Placement Eye;

  bool EyeStandsInside = false;

  size_t FramedParts = 0;

  std::vector<std::array<float, 3>> EmittedRadiance;

  std::vector<uint32_t> PartSurface;

  std::vector<std::array<double, 16>> PartPlacement;

  std::vector<Render::SubjectMaterial> Surfaces;

  const std::vector<double> *PreviousPositionsM = nullptr;

  std::vector<outshine::PunctualLight> Lights;

  Render::SubjectEnvironment Environment;
};

[[nodiscard]] bool Placed(Render::Renderer &renderer, const Studio &studio, std::string &error);

[[nodiscard]] bool Moved(Render::Renderer &renderer, size_t rows, size_t from, size_t to,
                         const double ecef[16], std::string &error);

struct StudioScratch {
  std::vector<float> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<Render::SubjectLight> Lights;
  Render::DrawList Draws;
};

[[nodiscard]] bool Aim(Render::Renderer &renderer, const Gltf::Subject &subject,
                       const Gltf::Placement &eye, std::string &error,
                       bool standsInside = false, size_t framedParts = 0);

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
