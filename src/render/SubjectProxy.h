#ifndef OUTSHINE_RENDER_SUBJECTPROXY_H
#define OUTSHINE_RENDER_SUBJECTPROXY_H

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

struct SubjectProxy {
  double AnchorEcefM[3] = {0.0, 0.0, 0.0};

  const Gltf::Subject *Geometry = nullptr;
  Gltf::Placement Eye;

  bool EyeStandsInside = false;

  size_t FramedParts = 0;

  std::vector<std::array<float, 3>> EmittedRadiance;

  std::vector<uint32_t> PartSurface;

  std::vector<std::array<double, 16>> PartPlacement;

  std::vector<SubjectMaterial> Surfaces;

  const std::vector<double> *PreviousPositionsM = nullptr;

  std::vector<outshine::PunctualLight> Lights;

  SubjectEnvironment Environment;
};

[[nodiscard]] bool Placed(Renderer &renderer, const SubjectProxy &proxy, std::string &error);

[[nodiscard]] bool Moved(Renderer &renderer, size_t rows, size_t from, size_t to,
                         const double ecef[16], std::string &error);

struct SubjectScratch {
  std::vector<float> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<SubjectLight> Lights;
  DrawList Draws;
};

[[nodiscard]] bool Aim(Renderer &renderer, const Gltf::Subject &subject,
                       const Gltf::Placement &eye, const double anchorEcefM[3], std::string &error,
                       bool standsInside = false, size_t framedParts = 0);

[[nodiscard]] bool Show(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
                        std::string &error);

[[nodiscard]] bool Surface(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
                           std::string &error);

[[nodiscard]] bool Place(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
                        std::string &error);

[[nodiscard]] bool Move(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
                        std::string &error);

}
#endif
