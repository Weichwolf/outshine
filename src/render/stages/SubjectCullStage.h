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

// ONE THREADGROUP PER CLUSTER, NOT ONE THREAD. The first version gave a cluster to a single thread
// and had it copy its own 384 indices in a loop, which is a serial memcpy on a machine built to do
// the opposite: Heidelberg's 7650 clusters became 7650 busy lanes and took the frame from 9.59 ms
// to 68.21. The copy itself is 23 MB a frame there, which is a quarter of a millisecond at this
// device's bandwidth -- everything above that was the shape of the kernel and not the work in it.
//
// So the group decides ONCE, in lane zero, and every lane copies a strided slice of what it kept.
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
