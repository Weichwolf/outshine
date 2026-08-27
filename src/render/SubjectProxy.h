#ifndef OUTSHINE_RENDER_SUBJECTPROXY_H
#define OUTSHINE_RENDER_SUBJECTPROXY_H

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "DrawList.h"
#include "Axes.h"
#include "Subject.h"
#include "SubjectDraw.h"

namespace outshine::Render {

class Renderer;

class SubjectProxy {
public:
  void Stands(const Gltf::Subject &subject, const double anchorEcefM[3]);
  void Sees(const Gltf::Placement &eye, bool standsInside, size_t framedParts);
  void Posed(const std::vector<double> *previousPositionsM) { Previous_ = previousPositionsM; }
  [[nodiscard]] bool Wears(std::span<const uint32_t> partSlot,
                           std::span<const SubjectMaterial> slots, std::string &error);
  [[nodiscard]] bool Emits(size_t part, const std::array<float, 3> &radiance);
  [[nodiscard]] bool Places(size_t part, const double m16[16]);
  void Lit(const outshine::PunctualLight &light) { Lights_.push_back(light); }
  void Around(const SubjectEnvironment &environment) { Environment_ = environment; }

  [[nodiscard]] const Gltf::Subject *Subject() const { return Subject_; }
  [[nodiscard]] size_t Parts() const { return PartPlacement_.size(); }
  [[nodiscard]] size_t Placements() const { return Placed_ ? PartPlacement_.size() : 0; }
  [[nodiscard]] const double *Anchor() const { return AnchorEcefM_; }
  [[nodiscard]] const Gltf::Placement &Eye() const { return Eye_; }
  [[nodiscard]] bool StandsInside() const { return StandsInside_; }
  [[nodiscard]] size_t FramedParts() const { return FramedParts_; }
  [[nodiscard]] const std::array<float, 3> &Emitted(size_t part) const {
    return EmittedRadiance_[part];
  }
  [[nodiscard]] uint32_t Slot(size_t part) const { return PartSurface_[part]; }
  [[nodiscard]] const std::array<double, 16> &Placement(size_t part) const {
    return PartPlacement_[part];
  }
  [[nodiscard]] std::span<const SubjectMaterial> Slots() const { return Surfaces_; }
  [[nodiscard]] const std::vector<double> *Previous() const { return Previous_; }
  [[nodiscard]] std::span<const outshine::PunctualLight> Lights() const { return Lights_; }
  [[nodiscard]] const SubjectEnvironment &Environment() const { return Environment_; }

private:
  double AnchorEcefM_[3] = {0.0, 0.0, 0.0};
  const Gltf::Subject *Subject_ = nullptr;
  Gltf::Placement Eye_;
  bool StandsInside_ = false;
  size_t FramedParts_ = 0;
  std::vector<std::array<float, 3>> EmittedRadiance_;
  std::vector<uint32_t> PartSurface_;
  std::vector<std::array<double, 16>> PartPlacement_;
  bool Placed_ = false;
  std::vector<SubjectMaterial> Surfaces_;
  const std::vector<double> *Previous_ = nullptr;
  std::vector<outshine::PunctualLight> Lights_;
  SubjectEnvironment Environment_;
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
