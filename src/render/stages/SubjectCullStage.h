#ifndef OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H
#define OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H

#include <cstdint>
#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "DepthPyramid.h"
#include "KernelShape.h"

namespace outshine::Render {

class SubjectDraw;
class PieceStore;
struct SubjectResidency;

class SubjectCullStage {
public:
  [[nodiscard]] static std::string KernelSource();
  [[nodiscard]] static std::string KernelSource(std::string &error);

  void PyramidFrom(SDL_GPUBuffer *pyramid, const PyramidShape &shape) {
    PyramidBuffer_ = pyramid;
    Pyramid_ = shape;
  }

  static constexpr ComputeShape CullShape{
      .ReadOnlyBuffers = 5, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64};
  static constexpr ComputeShape ScanShape{
      .ReadOnlyBuffers = 2, .ReadWriteBuffers = 2, .UniformBuffers = 1, .GroupX = 256};
  static constexpr ComputeShape CompactShape{
      .ReadOnlyBuffers = 4, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 128};

  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void CullsPieces(const PieceStore *pieces) { Pieces_ = pieces; }

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
  bool Stood_ = false;
  [[nodiscard]] static bool Pipeline(const Gpu &gpu,
                                     const char *entry,
                                     const ComputeShape &shape,
                                     OwnedComputePipeline &into,
                                     std::string &error);
  [[nodiscard]] uint32_t Standing(const FrameContext &ctx, void *view, uint32_t jobs);
  void CullOver(const void *view,
                uint32_t jobs,
                const SubjectResidency &resident,
                const PassRecording &into);
  void ScanOver(const void *view,
                uint32_t batches,
                const SubjectResidency &resident,
                const PassRecording &into);
  void CompactOver(const void *view,
                   uint32_t jobs,
                   const SubjectResidency &resident,
                   const PassRecording &into);
  const PieceStore *Pieces_ = nullptr;

  SubjectDraw *Subjects_ = nullptr;
  OwnedComputePipeline Cull_, Scan_, Compact_;
  uint32_t Swept_ = 0;
  float HeightPx_ = 0.0f;
};

} // namespace outshine::Render
#endif
