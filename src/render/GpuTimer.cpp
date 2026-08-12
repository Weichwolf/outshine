#include "GpuTimer.h"
#include "Log.h"

namespace outshine::Render {

void GpuTimer::Configure(const wgpu::Device &dev, bool featureGranted) {
  if (!featureGranted) {
    Log::Warn("render", "gputime_unavailable");
    return;
  }
  Dev_ = dev;

  wgpu::QuerySetDescriptor qd{};
  qd.type = wgpu::QueryType::Timestamp;
  qd.count = kQueries;
  Set_ = Dev_.CreateQuerySet(&qd);
  if (!Set_) { Log::Error("render", "gputime_queryset_failed"); return; }

  wgpu::BufferDescriptor rb{};
  rb.size = (uint64_t)kQueries * sizeof(uint64_t);
  rb.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
  Resolved_ = Dev_.CreateBuffer(&rb);

  wgpu::BufferDescriptor mb{};
  mb.size = rb.size;
  mb.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  for (int i = 0; i < kRing; i++) Read_[i] = Dev_.CreateBuffer(&mb);

  Log::Info("render", "gputime_on", {{"passes", (int)kPassCount}});
}

void GpuTimer::BeginFrame(void) {
  if (!Active()) return;
  for (int i = 0; i < kPassCount; i++) Used_[i] = false;
}

wgpu::PassTimestampWrites *GpuTimer::Writes(Pass p) {
  if (!Active() || p < 0 || p >= kPassCount) return nullptr;
  Writes_[p] = wgpu::PassTimestampWrites{};
  Writes_[p].querySet = Set_;
  Writes_[p].beginningOfPassWriteIndex = (uint32_t)(p * 2);
  Writes_[p].endOfPassWriteIndex = (uint32_t)(p * 2 + 1);
  Used_[p] = true;
  return &Writes_[p];
}

void GpuTimer::Resolve(wgpu::CommandEncoder &enc) {
  if (!Active()) return;
  enc.ResolveQuerySet(Set_, 0, kQueries, Resolved_, 0);
  /* A frame whose slot is still mapped skips the copy rather than stalling on it. The ring is three
   * deep, so this only fires when the GPU is a whole ring behind — and then a dropped sample is the
   * cheaper answer than a synchronised frame, which would measure the measurement. */
  if (!Busy_[Slot_])
    enc.CopyBufferToBuffer(Resolved_, 0, Read_[Slot_], 0, (uint64_t)kQueries * sizeof(uint64_t));
}

void GpuTimer::Poll(void) {
  if (!Active() || Busy_[Slot_]) return;
  const uint64_t bytes = (uint64_t)kQueries * sizeof(uint64_t);
  const int slot = Slot_;
  Busy_[slot] = true;
  Slot_ = (Slot_ + 1) % kRing;
  Read_[slot].MapAsync(
      wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::AllowSpontaneous,
      [this, slot](wgpu::MapAsyncStatus st, wgpu::StringView) {
        if (st == wgpu::MapAsyncStatus::Success) {
          const uint64_t *t =
              (const uint64_t *)Read_[slot].GetConstMappedRange(0, (uint64_t)kQueries * sizeof(uint64_t));
          if (t) {
            /* A pass that did not run leaves its pair unwritten; -1 says absent, which is a different
             * fact from 0.0 ms and must not be averaged with it. */
            uint64_t first = 0, last = 0;
            for (int i = 0; i < kPassCount; i++) {
              const bool wrote = Used_[i] && t[i * 2 + 1] > t[i * 2];
              Last_.PassMs[i] = wrote ? (double)(t[i * 2 + 1] - t[i * 2]) * 1.0e-6 : -1.0;
              if (!wrote) continue;
              if (!first || t[i * 2] < first) first = t[i * 2];
              if (t[i * 2 + 1] > last) last = t[i * 2 + 1];
            }
            /* The WHOLE encoder as one span, out of the pairs that are already there: the oldest
             * begin to the newest end. */
            Last_.FrameMs = last > first ? (double)(last - first) * 1.0e-6 : -1.0;
            Fresh_ = true;
          }
          Read_[slot].Unmap();
        }
        Busy_[slot] = false;
      });
}

bool GpuTimer::Take(Sample &out) {
  if (!Fresh_) return false;
  out = Last_;
  Fresh_ = false;
  return true;
}

} // namespace outshine::Render
