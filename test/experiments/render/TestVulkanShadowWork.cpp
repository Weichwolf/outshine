#include "VulkanShadowWork.h"
#include "Check.h"
#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>

using namespace outshine::Experiment;
using namespace outshine::Test;

namespace {
constexpr uint32_t kLarge = 600, kSmall = 300, kUnknown = 900;
constexpr uint64_t kRawTotal = 1800, kWeightedTotal = 3300, kInstances = 6;
constexpr ShadowPatchAddress kPatch{.Region = 1, .X = 2048, .Y = 0, .Width = 2, .Height = 2048};

void CountsAndSubmission() {
  const auto work = std::make_unique<ShadowWorkTable>();
  uint32_t head = kNoShadowPatch;
  work->Indexed(head,
                kPatch,
                {.IndexCount = kLarge, .Instances = 3, .VertexBindings = 1, .PipelineKnown = true});
  work->Indexed(head,
                kPatch,
                {.IndexCount = kSmall,
                 .Instances = 2,
                 .VertexBindings = 2,
                 .PipelineKnown = true,
                 .Fragment = true});
  work->Indexed(
      head,
      kPatch,
      {.IndexCount = kUnknown, .Instances = 1, .VertexBindings = 2, .PipelineKnown = false});
  work->NonIndexed(head, kPatch, 3, 1);
  work->Indirect(head, kPatch, 2);
  CHECK(work->Rows().size() == 1, "one command patch groups all its packet types");
  const auto &row = work->Rows().front();
  CHECK(row.IndexedPackets == 3 && row.IndexRequests == kRawTotal &&
            row.IndexedInstances == kInstances && row.InstanceIndexRequests == kWeightedTotal,
        "actual instance-weighted requests differ from raw index counts");
  CHECK(row.OneStreamRequests == uint64_t{kLarge} * 3 &&
            row.TwoStreamRequests == uint64_t{kSmall} * 2 && row.OtherStreamRequests == kUnknown &&
            row.UnknownPipelinePackets == 1 && row.FragmentRequests == uint64_t{kSmall} * 2,
        "missing pipeline metadata remains unknown instead of being called terrain");
  CHECK(row.NonIndexedPackets == 1 && row.InstanceVertexRequests == 3 && row.IndirectPackets == 1 &&
            row.IndirectRecords == 2,
        "indirect argument counts never invent indexed or vertex work");
  CHECK(row.Largest[0].Requests() == uint64_t{kLarge} * 3 &&
            row.Largest[1].Requests() == kUnknown &&
            row.Largest[2].Requests() == uint64_t{kSmall} * 2,
        "largest packets rank actual weighted work");
  auto other = kPatch;
  ++other.X;
  work->NonIndexed(head, other, 3, 1);
  CHECK(work->Rows().size() == 2, "different physical patches remain separate");
  work->Submitted(head, {.Serial = 1, .Queue = 1});
  CHECK(work->Rows()[0].Serial == 1 && work->Rows()[1].Serial == 1 && work->Rows()[0].Queue == 1 &&
            work->Dropped() == 0,
        "one accepted submission owns all its patch work");
  head = kNoShadowPatch;
  work->Indexed(head, kPatch, {.IndexCount = kLarge, .Instances = 0});
  CHECK(work->Rows().size() == 3 && work->Rows()[2].Serial == 0 &&
            work->Rows()[2].InstanceIndexRequests == 0,
        "command reuse opens new unsubmitted rows and zero instances request zero indices");
  work->Submitted(head, {.Serial = 2, .Queue = 1});
  CHECK(work->Rows()[0].Serial == 1 && work->Rows()[2].Serial == 2,
        "later command reuse cannot mutate an earlier submission");
  work->Submitted(head, {.Serial = 3, .Queue = 1});
  CHECK(work->Dropped() == 1 && work->Rows()[2].Serial == 2,
        "unsupported repeated command submission invalidates the trace");
}

void Limits() {
  const auto work = std::make_unique<ShadowWorkTable>();
  for (size_t at = 0; at < kShadowPatchesMost; ++at) {
    uint32_t head = kNoShadowPatch;
    work->NonIndexed(head, kPatch, 3, 1);
  }
  uint32_t head = kNoShadowPatch;
  work->NonIndexed(head, kPatch, 3, 1);
  CHECK(work->Rows().size() == kShadowPatchesMost && work->Dropped() == 1 && head == kNoShadowPatch,
        "table exhaustion is reported without replacing protected earlier records");
}

void OverflowAndInvalidAddress() {
  const auto work = std::make_unique<ShadowWorkTable>();
  uint32_t head = kNoShadowPatch;
  constexpr uint32_t maximum = std::numeric_limits<uint32_t>::max();
  const ShadowIndexedPacket huge{.IndexCount = maximum, .Instances = maximum};
  work->Indexed(head, kPatch, huge);
  work->Indexed(head, kPatch, huge);
  CHECK(work->Dropped() == 2 && work->Rows().front().InstanceIndexRequests == huge.Requests(),
        "unrepresentable total and unknown-family requests fail explicitly without wrapping");
  head = maximum - 1;
  work->NonIndexed(head, kPatch, 3, 1);
  CHECK(work->Dropped() == 3 && work->Rows().size() == 1,
        "invalid list heads never read beyond existing records");
}
}

int main() {
  CountsAndSubmission();
  Limits();
  OverflowAndInvalidAddress();
  return Report();
}
