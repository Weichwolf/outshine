#ifndef OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H
#define OUTSHINE_RENDER_STAGES_SUBJECTCULLSTAGE_H

#include <cstdint>
#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "KernelShape.h"

namespace outshine::Render {

class SubjectDraw;

class SubjectCullStage {
public:
  [[nodiscard]] static std::string KernelSource();
  [[nodiscard]] static std::string KernelSource(std::string &error);

  static constexpr ComputeShape CullShape{
      .ReadOnlyBuffers = 4, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64};
  static constexpr ComputeShape ScanShape{
      .ReadOnlyBuffers = 2, .ReadWriteBuffers = 2, .UniformBuffers = 1, .GroupX = 256};
  static constexpr ComputeShape CompactShape{
      .ReadOnlyBuffers = 4, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 128};

  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void EncodeCull(const FrameContext &ctx, const PassRecording &into);
  void EncodeScan(const FrameContext &ctx, const PassRecording &into);
  void EncodeCompact(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] uint32_t JobsSwept() const { return Swept_; }

private:
  [[nodiscard]] bool Pipeline(const Gpu &gpu,
                              const char *entry,
                              const ComputeShape &shape,
                              OwnedComputePipeline &into,
                              std::string &error);
  [[nodiscard]] uint32_t Standing(const FrameContext &ctx, void *view);

  SubjectDraw *Subjects_ = nullptr;
  OwnedComputePipeline Cull_, Scan_, Compact_;
  uint32_t Swept_ = 0;
};

} // namespace outshine::Render
#endif
