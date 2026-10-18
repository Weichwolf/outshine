#include "src/render/stages/SubjectCullStage.h"
#include "src/render/stages/GpuPlacement.h"
#include "src/render/device/ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Stream = SubjectResidency::Stream;
constexpr std::array<float, 16> kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
constexpr uint32_t kRejectedEvery = 5;
constexpr float kSphereRadius = 0.01f;

struct alignas(16) Job {
  uint32_t Unused = 0;
  uint32_t Batch = 0;
  uint32_t First = 0;
  uint32_t Count = 0;
};

static_assert(sizeof(Job) == 16 && alignof(Job) == 16);
static_assert(offsetof(Job, Batch) == 4 && offsetof(Job, First) == 8 && offsetof(Job, Count) == 12);

struct Inputs {
  std::vector<Job> Jobs;
  std::vector<uint32_t> Source;
  std::vector<uint32_t> Kept;
  std::vector<uint32_t> Batches;
  std::vector<uint32_t> Arguments;
  std::vector<uint32_t> ExpectedSlots;
  std::vector<uint32_t> ExpectedArguments;
  std::vector<uint32_t> ExpectedIndices;
};

Inputs MakeInputs(bool alternate) {
  constexpr uint32_t jobs = 701;
  constexpr uint32_t split = 300;
  constexpr uint32_t longEvery = 61;
  constexpr uint32_t longJob = 137;
  constexpr uint32_t countVariants = 7;
  constexpr uint32_t rejectEvery = 5;
  constexpr uint32_t sourceStride = 1000;
  constexpr std::size_t argumentsPerBatch = 5;
  Inputs input;
  input.Batches = {0, split, split, 0, split, jobs - split};
  input.Arguments.resize(argumentsPerBatch * 3, 0);
  input.ExpectedSlots.resize(jobs, std::numeric_limits<uint32_t>::max());
  uint32_t batchCount = 0;
  for (uint32_t at = 0; at < jobs; ++at) {
    if (at == split) {
      input.Arguments[2] = 0;
      input.Arguments[argumentsPerBatch + 2] = batchCount;
      input.Arguments[2 * argumentsPerBatch + 2] = batchCount;
      batchCount = 0;
    }
    const auto count = at % longEvery == 0 ? longJob : at % countVariants + 1;
    input.Jobs.push_back({.Unused = at,
                          .Batch = at < split ? 0u : 2u,
                          .First = static_cast<uint32_t>(input.Source.size()),
                          .Count = count});
    const bool kept = alternate ? at % rejectEvery == 0 : at % rejectEvery != 0;
    input.Kept.push_back(kept ? count : 0);
    for (uint32_t index = 0; index < count; ++index) {
      const auto value = at * sourceStride + index;
      input.Source.push_back(value);
      if (kept) { input.ExpectedIndices.push_back(value); }
    }
    if (kept) {
      input.ExpectedSlots[at] = batchCount;
      batchCount += count;
    }
  }
  for (std::size_t batch = 0; batch < 3; ++batch) {
    input.Arguments[argumentsPerBatch * batch + 1] = 1;
  }
  input.ExpectedArguments = input.Arguments;
  input.ExpectedArguments[0] = input.Arguments[argumentsPerBatch + 2];
  input.ExpectedArguments[argumentsPerBatch] = 0;
  input.ExpectedArguments[2 * argumentsPerBatch] = batchCount;
  return input;
}

std::vector<uint32_t>
Read(Device &device, SubjectResidency &resident, Stream stream, size_t count) {
  auto transfer = Require(RetryOffline(
      [&] {
        return ReadbackTransfer::Start(
            device, *resident.Buffer(stream), {.Bytes = count * sizeof(uint32_t)});
      },
      std::chrono::seconds(5)));
  const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "actual culling buffers complete within the explicit offline wait budget");
  if (!bytes) { Failed(); }
  std::vector<uint32_t> words(count);
  std::memcpy(words.data(), bytes->data(), count * sizeof(uint32_t));
  return words;
}

void Upload(SubjectResidency &resident, Stream stream, const void *data, size_t bytes) {
  SubjectResidency::Crossing crossing{.Which = stream,
                                      .Usage = BufferUse::Storage,
                                      .From = data,
                                      .Bytes = static_cast<uint32_t>(bytes)};
  Require(RetryOffline([&] { return resident.Cross(std::span(&crossing, 1), true); },
                       std::chrono::seconds(5)));
}

void Run(Device &device, SubjectCullStage &stage, const FrameContext &ctx, bool submit) {
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  StageSubmission submission;
  const PassRecording recording{
      .Commands = commands.get(), .Submission = submission, .Area = {}, .Clip = {}};
  Require(stage.EncodeCull(ctx, recording));
  Require(stage.EncodeScan(recording));
  Require(stage.EncodeCompact(recording));
  if (submit) {
    const auto fence = Require(device.Submit(std::move(commands)));
    submission.Commit();
    CHECK(Require(device.Wait(fence, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "the actual Cull/Scan/Compact chain completes");
  }
}

void Compare(Device &device, SubjectResidency &resident, const Inputs &expected) {
  CHECK(Read(device, resident, Stream::ClusterSlot, expected.ExpectedSlots.size()) ==
            expected.ExpectedSlots,
        "all accepted and rejected jobs retain independently computed compacted slots");
  auto expectedArguments = expected.ExpectedArguments;
  const auto original = MakeInputs(false);
  for (size_t batch = 0; batch < 3; ++batch) {
    expectedArguments[batch * 5 + 2] = original.Arguments[batch * 5 + 2];
  }
  CHECK(Read(device, resident, Stream::DrawArguments, expectedArguments.size()) ==
            expectedArguments,
        "indirect arguments preserve the empty batch and original resident draw bases");
  const auto indices = Read(device, resident, Stream::DrawIndex, expected.Source.size());
  bool equal = true;
  for (size_t at = 0; at < expected.Jobs.size(); ++at) {
    if (expected.Kept[at] == 0) { continue; }
    const auto &job = expected.Jobs[at];
    const auto base = original.Arguments[job.Batch * 5 + 2] + expected.ExpectedSlots[at];
    for (uint32_t i = 0; i < job.Count; ++i) {
      equal = equal && indices[base + i] == expected.Source[job.First + i];
    }
  }
  CHECK(equal, "all actually drawn index ranges match the independent ordered reference");
}

void Reuse(SubjectCullStage &stage, const FrameContext &ctx) {
  StageSubmission submission;
  const PassRecording recording{.Submission = submission, .Area = {}, .Clip = {}};
  Require(stage.EncodeCull(ctx, recording));
  Require(stage.EncodeScan(recording));
  Require(stage.EncodeCompact(recording));
  CHECK(stage.JobsSwept() == 0,
        "an unchanged accepted view needs no command recording or GPU work");
}

void Exercise(Device &device) {
  const auto input = MakeInputs(false);
  SubjectResidency resident;
  Require(resident.StandsOn(&device));
  std::vector<float> spheres(input.Jobs.size() * 12, 0);
  for (size_t i = 0; i < input.Jobs.size(); ++i) {
    spheres[i * 12] = i % kRejectedEvery == 0 ? 3.0f : 0.0f;
    spheres[i * 12 + 2] = 0.5f;
    spheres[i * 12 + 3] = kSphereRadius;
    spheres[i * 12 + 9] = std::numeric_limits<float>::infinity();
  }
  GpuPlacement placement;
  placement.Current = kIdentity;
  Upload(resident, Stream::ClusterSpheres, spheres.data(), spheres.size() * sizeof(float));
  Upload(resident, Stream::ClusterJobs, input.Jobs.data(), input.Jobs.size() * sizeof(Job));
  Upload(resident, Stream::Placements, &placement, sizeof(placement));
  Upload(resident, Stream::Index, input.Source.data(), input.Source.size() * sizeof(uint32_t));
  Upload(resident,
         Stream::ClusterBatches,
         input.Batches.data(),
         input.Batches.size() * sizeof(uint32_t));
  Upload(resident,
         Stream::DrawArguments,
         input.Arguments.data(),
         input.Arguments.size() * sizeof(uint32_t));
  Require(RetryOffline([&] { return resident.SubmitPendingUploads(); }, std::chrono::seconds(5)));
  for (auto stream : {Stream::ClusterKept, Stream::ClusterSlot}) {
    Require(resident.Grow(stream,
                          {.Usage = BufferUse::Storage,
                           .Bytes = static_cast<uint32_t>(input.Jobs.size() * sizeof(uint32_t))}));
  }
  Require(resident.Grow(Stream::DrawIndex,
                        {.Usage = BufferUse::Storage | BufferUse::Index,
                         .Bytes = static_cast<uint32_t>(input.Source.size() * sizeof(uint32_t))}));
  SubjectCullStage stage;
  Require(stage.Configure({.Resident = &resident,
                           .AnchorM = {},
                           .Jobs = static_cast<uint32_t>(input.Jobs.size()),
                           .BatchRows = 3,
                           .Generation = 1,
                           .GroundGeneration = 1},
                          {.Device = &device, .SceneColours = {}}));
  FrameContext frame;
  frame.Mvp.Column = kIdentity;
  Run(device, stage, frame, false);
  Run(device, stage, frame, true);
  CHECK(stage.JobsSwept() == input.Jobs.size(),
        "an abandoned recording cannot suppress the next actual sweep");
  Compare(device, resident, input);
  Reuse(stage, frame);
  static_cast<void>(resident.TakeUploadAttempts());
  static_cast<void>(resident.TakeStagingAllocationAttempts());
  frame.PreViewTranslation[0] = -3;
  Run(device, stage, frame, true);
  const auto translated = MakeInputs(true);
  Compare(device, resident, translated);
  CHECK(resident.TakeUploadAttempts() == 0 && resident.TakeStagingAllocationAttempts() == 0,
        "camera movement changes visibility without regenerating or uploading world content");
  const auto shape = PyramidOver({.WidthPx = 37, .HeightPx = 25});
  std::vector<float> covered(shape.Texels, 1);
  Upload(resident, Stream::Previous, covered.data(), covered.size() * sizeof(float));
  Require(RetryOffline([&] { return resident.SubmitPendingUploads(); }, std::chrono::seconds(5)));
  stage.PyramidFrom(resident.Buffer(Stream::Previous).get(), shape);
  Run(device, stage, frame, true);
  Compare(device, resident, translated);
  Run(device, stage, frame, true);
  const auto occluded = Read(device, resident, Stream::DrawArguments, input.Arguments.size());
  for (size_t batch = 0; batch < 3; ++batch) {
    CHECK(occluded[batch * 5] == 0,
          "the preserved previous depth rejects hidden clusters on the stable refinement");
  }
  Reuse(stage, frame);
  frame.PreViewTranslation[0] = 0;
  Run(device, stage, frame, true);
  Compare(device, resident, input);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Exercise(*device);
  return Report();
}
