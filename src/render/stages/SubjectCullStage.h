#ifndef OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H
#define OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H

#include <cstdint>
#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "DepthPyramid.h"
#include "ComputeShaders.h"

namespace outshine::Render {

class SubjectDraw;

class SubjectCullStage {
public:
  void PyramidFrom(SDL_GPUBuffer *pyramid, const PyramidShape &shape) {
    if (PyramidBuffer_ != pyramid || Pyramid_.Wide != shape.Wide || Pyramid_.High != shape.High) {
      HasResult_ = false;
    }
    PyramidBuffer_ = pyramid;
    Pyramid_ = shape;
  }

  static constexpr ComputeShaderId CullShader = ComputeShaderId::SubjectCull;
  static constexpr ComputeShape CullShape = FindComputeShader(CullShader)->Shape;
  static constexpr ComputeShaderId ScanShader = ComputeShaderId::SubjectScan;
  static constexpr ComputeShape ScanShape = FindComputeShader(ScanShader)->Shape;
  static constexpr ComputeShaderId CompactShader = ComputeShaderId::SubjectCompact;
  static constexpr ComputeShape CompactShape = FindComputeShader(CompactShader)->Shape;

  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void Binds(SubjectDraw &subjects) noexcept {
    if (Subjects_ != &subjects) { HasResult_ = false; }
    Subjects_ = &subjects;
  }

  void EncodeCull(const FrameContext &ctx, const PassRecording &into);
  void EncodeScan(const FrameContext &ctx, const PassRecording &into);
  void EncodeCompact(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] uint32_t JobsSwept() const { return Swept_; }

  void Projects(float heightPx) { HeightPx_ = heightPx; }

  [[nodiscard]] static float ErrorPerMetreTaken();

  [[nodiscard]] static uint32_t JobsSweptTaken();

private:
  SDL_GPUBuffer *PyramidBuffer_ = nullptr;
  PyramidShape Pyramid_;
  bool HasResult_ = false;
  bool CullThisFrame_ = false;
  bool OccludeThisFrame_ = false;
  Mat4f LastMvp_{};
  Vec3 LastPreViewTranslation_{};
  uint64_t LastSubjectGeneration_ = 0;
  [[nodiscard]] static bool EnsurePipeline(const Gpu &gpu,
                                           ComputeShaderId shader,
                                           OwnedComputePipeline &into,
                                           std::string &error);
  [[nodiscard]] uint32_t Standing(const FrameContext &ctx, void *view);

  SubjectDraw *Subjects_ = nullptr;
  OwnedComputePipeline Cull_, Scan_, Compact_;
  uint32_t Swept_ = 0;
  float HeightPx_ = 0.0f;
};

}
#endif
