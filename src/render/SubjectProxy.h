#ifndef OUTSHINE_RENDER_SUBJECTPROXY_H
#define OUTSHINE_RENDER_SUBJECTPROXY_H

#include "math/Mat4.h"
#include "math/Vec3.h"
#include "Shape.h"
#include "Viewing.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "DrawList.h"
#include "CameraState.h"
#include "SubjectDraw.h"

namespace outshine::Render {

class SceneRenderer;

class SubjectProxy {
public:
  void Stands(const Shape &subject, const Vec3 &anchorEcefM);

  void Posed(std::span<const float> previousPositionsM) { Previous_ = previousPositionsM; }

  [[nodiscard]] bool Wears(std::span<const uint32_t> partSlot,
                           std::span<const SubjectMaterial> slots,
                           std::string &error);
  [[nodiscard]] bool Emits(size_t part, const std::array<float, 3> &radiance);
  [[nodiscard]] bool Places(size_t part, const Mat4 &placement);
  [[nodiscard]] bool Places(size_t part, size_t instance, const Mat4 &placement);
  [[nodiscard]] bool Carries(size_t instances);

  void Lit(const outshine::PunctualLight &light) { Lights_.push_back(light); }

  void Around(const SubjectEnvironment &environment) { Environment_ = environment; }

  [[nodiscard]] const Shape *Shaped() const { return Shape_; }

  [[nodiscard]] size_t Parts() const {
    return Instances_ == 0 ? 0 : PartPlacement_.size() / Instances_;
  }

  [[nodiscard]] size_t Instances() const { return Instances_; }

  [[nodiscard]] size_t Placements() const { return Placed_ ? PartPlacement_.size() : 0; }

  [[nodiscard]] const Vec3 &Anchor() const { return AnchorEcefM_; }

  [[nodiscard]] const std::array<float, 3> &Emitted(size_t part) const {
    return EmittedRadiance_[part];
  }

  [[nodiscard]] uint32_t Slot(size_t part) const { return PartSurface_[part]; }

  [[nodiscard]] const Mat4 &Placement(size_t row) const { return PartPlacement_[row]; }

  [[nodiscard]] std::span<const SubjectMaterial> Slots() const { return Surfaces_; }

  [[nodiscard]] const std::span<const float> &Previous() const { return Previous_; }

  [[nodiscard]] std::span<const outshine::PunctualLight> Lights() const { return Lights_; }

  [[nodiscard]] const SubjectEnvironment &IndirectLight() const { return Environment_; }

private:
  Vec3 AnchorEcefM_;
  const Shape *Shape_ = nullptr;
  std::vector<std::array<float, 3>> EmittedRadiance_;
  std::vector<uint32_t> PartSurface_;
  std::vector<Mat4> PartPlacement_;
  size_t Instances_ = 1;
  bool Placed_ = false;
  std::vector<SubjectMaterial> Surfaces_;
  std::span<const float> Previous_;
  std::vector<outshine::PunctualLight> Lights_;
  SubjectEnvironment Environment_;
};

[[nodiscard]] bool Placed(SceneRenderer &renderer, const SubjectProxy &proxy, std::string &error);

struct Moving {
  size_t Rows = 0;
  size_t Many = 1;
  size_t Which = 0;
  size_t FromPart = 0;
  size_t ToPart = 0;
};

[[nodiscard]] bool
Moved(SceneRenderer &renderer, Moving what, const Mat4 &ecef, std::string &error);

struct SubjectTransferMetrics {
  static constexpr uint64_t kDigestMask = 0xffffffffffffull;
  uint64_t GeometryDigest = 0;
  double PackingMs = 0;
  double DigestMs = 0;
  double UploadMs = 0;
  double MeshAdmissionMs = 0;
  double IndexUploadMs = 0;
  double StreamUploadMs = 0;
  double TableUploadMs = 0;

  [[nodiscard]] double DigestValue() const {
    return static_cast<double>(GeometryDigest & kDigestMask);
  }

  [[nodiscard]] bool operator==(const SubjectTransferMetrics &) const = default;
};

struct SubjectScratch {
  SubjectTransferMetrics Metrics;

  bool Digests = false;
  std::vector<float> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<SubjectLight> Lights;
  DrawList Draws;
};

[[nodiscard]] bool Aim(SceneRenderer &renderer,
                       const Shape &subject,
                       const SubjectView &view,
                       const Vec3 &anchorEcefM,
                       std::string &error);

[[nodiscard]] bool Show(SceneRenderer &renderer,
                        const SubjectProxy &proxy,
                        const SubjectView &view,
                        SubjectScratch &scratch,
                        std::string &error);

[[nodiscard]] bool Surface(SceneRenderer &renderer,
                           const SubjectProxy &proxy,
                           const SubjectView &view,
                           SubjectScratch &scratch,
                           std::string &error);

bool Place(SceneRenderer &renderer,
           const SubjectProxy &proxy,
           const SubjectView &view,
           SubjectScratch &scratch,
           std::string &error);

[[nodiscard]] bool Move(SceneRenderer &renderer,
                        const SubjectProxy &proxy,
                        const SubjectView &view,
                        SubjectScratch &scratch,
                        std::string &error);

}
#endif
