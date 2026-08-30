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

  static constexpr ComputeShape KernelShape{
      .ReadOnlyBuffers = 4, .ReadWriteBuffers = 2, .UniformBuffers = 1, .GroupX = 128};

  [[nodiscard]] bool Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error);

  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] uint32_t JobsSwept() const { return Swept_; }

private:
  SubjectDraw *Subjects_ = nullptr;
  OwnedComputePipeline Pipe_;
  uint32_t Swept_ = 0;
};

}
#endif
