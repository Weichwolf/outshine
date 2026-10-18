#ifndef OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H
#define OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H

#include "FrameContext.h"
#include "Gpu.h"
#include "DepthPyramid.h"
#include "ComputeShaders.h"
#include "StageSubmission.h"
#include "SubjectResidency.h"
#include "math/Vec4.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace outshine::Render {

class SubjectCullStage {
public:
  struct Source {
    const SubjectResidency *Resident = nullptr;
    Vec3 AnchorM{};
    uint32_t Jobs = 0;
    uint32_t BatchRows = 0;
    uint64_t Generation = 0;
    uint64_t GroundGeneration = 0;
  };

  void PyramidFrom(Native::Buffer *pyramid, const PyramidShape &shape) {
    if (PyramidBuffer_ != pyramid || Pyramid_.Wide != shape.Wide || Pyramid_.High != shape.High ||
        Pyramid_.SourceWide != shape.SourceWide || Pyramid_.SourceHigh != shape.SourceHigh) {
      Cache_.Invalidate();
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

  [[nodiscard]] Native::Result<void> Configure(Source source, const Gpu &gpu);
  void Binds(Source source) noexcept;

  void Invalidate() noexcept { Cache_.Invalidate(); }

  [[nodiscard]] Native::Result<void> EncodeCull(const FrameContext &ctx, const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeScan(const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeCompact(const PassRecording &into);

  [[nodiscard]] uint32_t JobsSwept() const { return Swept_; }

  void Projects(float heightPx) { HeightPx_ = heightPx; }

  [[nodiscard]] static float ErrorPerMetreTaken();
  [[nodiscard]] static uint32_t JobsSweptTaken();

private:
  static constexpr size_t kPlaneFloats = 24;
  static constexpr size_t kPyramidOffset = 192;
  static constexpr size_t kSourceOffset = 244;

  struct alignas(16) CullView {
    std::array<float, kPlaneFloats> Planes{};
    Vec4f Shift;
    uint32_t Jobs;
    float ErrorPerMetre;
    std::array<uint32_t, 2> Pad{};
    Mat4f Clip{};
    std::array<uint32_t, 4> PyramidWide{};
    std::array<uint32_t, 4> PyramidHigh{};
    std::array<uint32_t, 4> PyramidAt{};
    uint32_t Occludes;
    uint32_t SourceWide;
    uint32_t SourceHigh;
    uint32_t Pad2 = 0;
  };

  static_assert(sizeof(CullView) == 256 && alignof(CullView) == 16);
  static_assert(offsetof(CullView, Clip) == 128);
  static_assert(offsetof(CullView, PyramidWide) == kPyramidOffset);
  static_assert(offsetof(CullView, SourceWide) == kSourceOffset);
  static_assert(offsetof(CullView, SourceHigh) == kSourceOffset + sizeof(uint32_t));

  Native::Buffer *PyramidBuffer_ = nullptr;
  PyramidShape Pyramid_;
  StageCache Cache_;
  bool CullThisFrame_ = false;
  bool OccludeThisFrame_ = false;
  bool Refined_ = false;
  Mat4f LastMvp_{};
  Vec3 LastPreViewTranslation_{};
  uint64_t LastSubjectGeneration_ = 0;
  uint64_t LastGroundGeneration_ = 0;
  [[nodiscard]] static Native::Result<void>
  EnsurePipeline(Native::Device &device,
                 ComputeShaderId shader,
                 std::unique_ptr<Native::ComputePipeline> &into);
  void Standing(const FrameContext &ctx);

  Source Source_;
  CullView View_{};
  std::unique_ptr<Native::ComputePipeline> Cull_, Scan_, Compact_;
  uint32_t Swept_ = 0;
  float HeightPx_ = 0;
};

}
#endif
