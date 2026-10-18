#include "SubjectDraw.h"
#include "ReadbackTransfer.h"
#include "Check.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
namespace Native = outshine::Render::Native;
using Failure = outshine::Test::Gpu::UploadFailurePoint;
outshine::Test::Gpu::UploadFailure fault{.Message = "injected subject upload failure"};

void Arm(Failure point) {
  fault.Point = point;
  fault.Skip = 0;
  fault.Armed = true;
}

bool Accepted(Native::Result<void> result, std::string &error) {
  if (!result) { error = result.error().Message; }
  return result.has_value();
}

std::vector<uint32_t> Read(Native::Device *device, const Native::Buffer *buffer, size_t words) {
  CHECK(buffer != nullptr, "GPU table buffer exists before readback");
  if (buffer == nullptr) { return {}; }
  auto reader = ReadbackTransfer::Start(*device, *buffer, {.Bytes = words * sizeof(uint32_t)});
  CHECK(reader.has_value(), "GPU table readback starts");
  if (!reader) { return {}; }
  const auto bytes = reader->Wait(*device, std::chrono::seconds(5));
  CHECK(bytes && *bytes, "GPU table readback completes within its offline budget");
  if (!bytes || !*bytes) { return {}; }
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), (**bytes).data(), result.size() * sizeof(uint32_t));
  return result;
}

void DeferredRetry(Native::Device *device) {
  SubjectResidency residency;
  CHECK(residency.StandsOn(device), "residency binds the native fixture device");
  const std::vector<uint32_t> initial{0, 0, 0, 0};
  const std::vector<uint32_t> expected{17, 23, 31, 47};
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  std::array<SubjectResidency::Crossing, 1> crossing{{{.Which = stream,
                                                       .Usage = Native::BufferUse::Storage,
                                                       .From = initial.data(),
                                                       .Bytes = 16}}};
  std::string error;
  CHECK(Accepted(residency.Cross(crossing, false), error), "deferred fixture initializes GPU data");
  crossing[0].From = expected.data();
  CHECK(Accepted(residency.Cross(crossing, true), error), "replacement is staged");
  auto failed = device->Begin();
  CHECK(failed.has_value(), "failed pass acquires commands");
  if (!failed) { return; }
  Arm(Failure::Copy);
  fault.Skip = 0;
  CHECK(!Accepted(residency.FlushCrossings(failed->get()), error) && error.contains("injected"),
        "copy pass failure is reported without consuming pending uploads");
  fault.Armed = false;
  failed->reset();
  auto cancelled = device->Begin();
  CHECK(cancelled.has_value(), "cancelled recording acquires commands");
  if (!cancelled) { return; }
  CHECK(Accepted(residency.FlushCrossings(cancelled->get()), error),
        "pending upload recording succeeds");
  cancelled->reset();
  CHECK(Read(device, residency.Buffer(stream).get(), initial.size()) == initial,
        "cancelled commands leave initial GPU contents intact");
  auto retry = device->Begin();
  CHECK(retry.has_value(), "retry acquires commands");
  if (!retry) { return; }
  CHECK(Accepted(residency.FlushCrossings(retry->get()), error),
        "pending upload recording succeeds");
  CHECK(device->Submit(std::move(*retry)), "retry submission succeeds");
  residency.CommitCrossings();
  CHECK(Read(device, residency.Buffer(stream).get(), expected.size()) == expected,
        "retry records the unacknowledged upload again");
}

void FailedReplacement(Native::Device *device) {
  SubjectResidency residency;
  CHECK(residency.StandsOn(device), "residency binds the native fixture device");
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  const std::vector<uint32_t> initial{17, 23, 31, 47};
  const std::vector<uint32_t> replacement{53, 59, 61, 67, 71, 73, 79, 83};
  std::array<SubjectResidency::Crossing, 1> crossing{{{.Which = stream,
                                                       .Usage = Native::BufferUse::Storage,
                                                       .From = initial.data(),
                                                       .Bytes = 16}}};
  std::string error;
  CHECK(Accepted(residency.Cross(crossing, false), error), "replacement fixture initializes data");
  const auto *original = residency.Buffer(stream).get();
  const auto capacity = residency.HeldOf(stream);
  crossing[0].From = replacement.data();
  crossing[0].Bytes = 32;
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    Arm(Failure::DeviceBuffer);
    fault.Skip = 0;
    CHECK(!Accepted(residency.Cross(crossing, false), error),
          "failed replacement reports allocation error");
    CHECK(!fault.Armed && error.contains("injected"),
          "replacement preserves allocation failure cause");
    fault.Armed = false;
    CHECK(residency.Buffer(stream).get() == original && residency.HeldOf(stream) == capacity,
          "allocation failure preserves original buffer identity and capacity");
    CHECK(Read(device, residency.Buffer(stream).get(), initial.size()) == initial,
          "allocation failure preserves original GPU data");
  }
  CHECK(Accepted(residency.Cross(crossing, false), error), "replacement retries successfully");
  CHECK(Read(device, residency.Buffer(stream).get(), replacement.size()) == replacement,
        "successful replacement publishes all new GPU words");
}

void FailedBatchReplacement(Native::Device *device) {
  SubjectResidency residency;
  CHECK(residency.StandsOn(device), "residency binds the native fixture device");
  using Stream = SubjectResidency::Stream;
  const std::vector<uint32_t> initial{17, 23, 31, 47};
  const std::vector<uint32_t> replacement{53, 59, 61, 67, 71, 73, 79, 83};
  std::array<SubjectResidency::Crossing, 2> crossing{{{.Which = Stream::ClusterJobs,
                                                       .Usage = Native::BufferUse::Storage,
                                                       .From = initial.data(),
                                                       .Bytes = 16},
                                                      {.Which = Stream::ClusterBatches,
                                                       .Usage = Native::BufferUse::Storage,
                                                       .From = initial.data(),
                                                       .Bytes = 16}}};
  std::string error;
  CHECK(Accepted(residency.Cross(crossing, false), error),
        "batch fixture initializes both buffers");
  std::array<Native::Buffer *, 2> originals{residency.Buffer(crossing[0].Which).get(),
                                            residency.Buffer(crossing[1].Which).get()};
  for (auto &item : crossing) {
    item.From = replacement.data();
    item.Bytes = 32;
  }
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    Arm(Failure::DeviceBuffer);
    fault.Skip = 1;
    CHECK(!Accepted(residency.Cross(crossing, false), error),
          "second allocation rejects the whole preparation");
    CHECK(!fault.Armed, "second allocation fault is reached");
    fault.Armed = false;
    for (size_t at = 0; at < crossing.size(); ++at) {
      CHECK(residency.Buffer(crossing[at].Which).get() == originals[at] &&
                residency.HeldOf(crossing[at].Which) == 16,
            "preparation failure preserves every original buffer");
      CHECK(Read(device, residency.Buffer(crossing[at].Which).get(), initial.size()) == initial,
            "preparation failure preserves every original GPU value");
    }
  }
  for (const Failure point : {Failure::Map, Failure::Acquire, Failure::Copy, Failure::Submit}) {
    Arm(point);
    fault.Skip = 0;
    CHECK(!Accepted(residency.Cross(crossing, false), error),
          "upload failure rejects prepared replacements");
    CHECK(!fault.Armed && error.contains("injected"),
          "replacement reaches the injected upload failure");
    fault.Armed = false;
    for (size_t at = 0; at < crossing.size(); ++at) {
      CHECK(residency.Buffer(crossing[at].Which).get() == originals[at] &&
                residency.HeldOf(crossing[at].Which) == 16,
            "upload failure restores every original buffer and capacity");
      CHECK(Read(device, residency.Buffer(crossing[at].Which).get(), initial.size()) == initial,
            "upload failure retains original GPU contents");
    }
  }
  CHECK(Accepted(residency.Cross(crossing, false), error),
        "batch replacement retries successfully");
  for (const auto &item : crossing) {
    CHECK(Read(device, residency.Buffer(item.Which).get(), replacement.size()) == replacement,
          "successful batch publishes every replacement");
  }
}

void MixedUploads(Native::Device *device) {
  SubjectResidency residency;
  CHECK(residency.StandsOn(device), "residency binds the native fixture device");
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  const std::vector<uint32_t> initial{0, 0, 0, 0};
  const std::vector<uint32_t> staged{17, 23, 31, 47};
  const std::vector<uint32_t> latest{53, 59, 61, 67};
  std::array<SubjectResidency::Crossing, 1> crossing{{{.Which = stream,
                                                       .Usage = Native::BufferUse::Storage,
                                                       .From = initial.data(),
                                                       .Bytes = 16}}};
  std::string error;
  CHECK(Accepted(residency.Cross(crossing, false), error), "mixed fixture initializes data");
  crossing[0].From = staged.data();
  CHECK(Accepted(residency.Cross(crossing, true), error), "earlier upload is deferred");
  crossing[0].From = latest.data();
  CHECK(Accepted(residency.Cross(crossing, false), error), "later upload is immediate");
  auto commands = device->Begin();
  CHECK(commands.has_value(), "mixed upload recording acquires commands");
  if (!commands) { return; }
  CHECK(Accepted(residency.FlushCrossings(commands->get()), error),
        "pending upload recording succeeds");
  CHECK(device->Submit(std::move(*commands)), "remaining deferred work submits");
  residency.CommitCrossings();
  CHECK(Read(device, residency.Buffer(stream).get(), latest.size()) == latest,
        "earlier deferred data cannot overwrite a later immediate upload");
  crossing[0].From = staged.data();
  CHECK(Accepted(residency.Cross(crossing, true), error), "growth has pending data to preserve");
  const auto *original = residency.Buffer(stream).get();
  const auto capacity = residency.HeldOf(stream);
  for (const Failure point : {Failure::Acquire, Failure::Copy, Failure::Submit}) {
    Arm(point);
    fault.Skip = 0;
    CHECK(!Accepted(
              residency.Grow(stream, {.Usage = Native::BufferUse::Storage, .Bytes = capacity * 2}),
              error),
          "pending submission failure prevents growth");
    CHECK(!fault.Armed, "growth reaches pending submission failure");
    fault.Armed = false;
    CHECK(residency.Buffer(stream).get() == original && residency.HeldOf(stream) == capacity,
          "failed pending submission preserves residency");
    CHECK(Read(device, original, latest.size()) == latest,
          "failed pending submission leaves the submitted GPU version intact");
  }
  CHECK(
      Accepted(residency.Grow(stream, {.Usage = Native::BufferUse::Storage, .Bytes = capacity * 2}),
               error),
      "growth retries pending uploads before copying");
  CHECK(Read(device, residency.Buffer(stream).get(), staged.size()) == staged,
        "grown buffer contains pending values instead of the obsolete submitted version");
}

void Growth(Native::Device *device) {
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  constexpr auto usage = Native::BufferUse::Storage;
  const std::vector<uint32_t> expected{17, 23, 31, 47};
  for (const Failure point : {Failure::Acquire, Failure::Copy, Failure::Submit}) {
    SubjectResidency residency;
    CHECK(residency.StandsOn(device), "residency binds the native fixture device");
    std::string error;
    std::array<SubjectResidency::Crossing, 1> crossing{
        {{.Which = stream,
          .Usage = usage,
          .From = expected.data(),
          .Bytes = static_cast<uint32_t>(expected.size() * sizeof(uint32_t))}}};
    CHECK(Accepted(residency.Cross(crossing, false), error), "growth fixture uploads known words");
    const auto *original = residency.Buffer(stream).get();
    const auto capacity = residency.HeldOf(stream);
    const auto maximum = std::numeric_limits<uint32_t>::max();
    for (const auto invalid :
         {SubjectResidency::Crossing{.Which = SubjectResidency::Stream::Count},
          SubjectResidency::Crossing{.Which = stream,
                                     .Usage = usage,
                                     .From = expected.data(),
                                     .Bytes = 16,
                                     .Offset = maximum - 7},
          SubjectResidency::Crossing{
              .Which = stream, .Usage = usage, .From = expected.data(), .Bytes = maximum}}) {
      std::array<SubjectResidency::Crossing, 2> rejected{{{.Which = stream}, invalid}};
      const auto allocations = fault.DeviceBufferRequests;
      CHECK(!Accepted(residency.Cross(rejected, false), error),
            "invalid upload is rejected before mutation");
      CHECK(fault.DeviceBufferRequests == allocations, "invalid upload performs no GPU allocation");
      CHECK(residency.Buffer(stream).get() == original && residency.HeldOf(stream) == capacity,
            "invalid later crossing cannot release an earlier buffer");
      CHECK(Read(device, original, expected.size()) == expected,
            "preflight rejection preserves existing GPU data");
    }
    auto large = crossing[0];
    large.Bytes = (maximum / 2u) + 1u;
    std::array<SubjectResidency::Crossing, 2> oversized{{large, large}};
    for (const bool deferred : {false, true}) {
      const auto allocations = fault.DeviceBufferRequests;
      CHECK(!Accepted(residency.Cross(oversized, deferred), error),
            "sum of individually representable uploads is checked");
      CHECK(fault.DeviceBufferRequests == allocations, "sum overflow performs no GPU allocation");
      CHECK(residency.Buffer(stream).get() == original && residency.HeldOf(stream) == capacity,
            "sum overflow preserves residency");
    }
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
      Arm(point);
      fault.Skip = 0;
      const unsigned before = fault.Rejected;
      CHECK(!Accepted(residency.Grow(stream, {.Usage = usage, .Bytes = capacity * 2}), error),
            "failed preservation copy rejects growth");
      CHECK(fault.Rejected == before + 1 && error.contains("injected"),
            "growth reports the injected copy failure");
      fault.Armed = false;
      CHECK(residency.Buffer(stream).get() == original && residency.HeldOf(stream) == capacity,
            "failed growth preserves buffer identity and capacity");
      CHECK(Read(device, original, expected.size()) == expected,
            "failed growth preserves original GPU contents");
    }
    CHECK(Accepted(residency.Grow(stream, {.Usage = usage, .Bytes = capacity * 2}), error),
          "growth succeeds after repeated failures");
    CHECK(residency.HeldOf(stream) >= capacity * 2,
          "successful growth publishes requested capacity");
    CHECK(Read(device, residency.Buffer(stream).get(), expected.size()) == expected,
          "successful growth copies all existing GPU words");
  }
}

enum class TablePhase { Staging, Submission };

struct TableRetry {
  SubjectDraw &Draw;
  Native::Device *Device;
  uint32_t Piece;
  const Mat4 &Row;
  std::string &Error;

  void Check(Failure point, unsigned after, TablePhase phase) {
    const std::array<Mat4, 1> one{Row};
    const std::array<Mat4, 2> two{Row, Row};
    CHECK(Draw.SetPieceInstances(Piece, one, Error) && Draw.HandTables(Error) &&
              Accepted(Draw.Owned().SubmitPendingUploads(), Error),
          "baseline has one submitted instance");
    CHECK(Draw.SetPieceInstances(Piece, two, Error), "two instances require different tables");
    if (phase == TablePhase::Submission) {
      CHECK(Draw.HandTables(Error), "the native tables stage before their command submission");
    }
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
      const unsigned before = fault.Rejected;
      Arm(point);
      fault.Skip = after;
      Error.clear();
      const bool accepted = phase == TablePhase::Staging
                                ? Draw.HandTables(Error)
                                : Accepted(Draw.Owned().SubmitPendingUploads(), Error);
      CHECK(!accepted, "table upload rejects every injected attempt");
      CHECK(fault.Rejected == before + 1 && Error.contains("injected"),
            "retry reaches the failing native operation and preserves its cause");
      if (phase == TablePhase::Staging) {
        CHECK(Draw.ClusterJobs() == 0 && Draw.ClusterBatchRows() == 0,
              "failed staging advertises no usable CPU job table");
      } else {
        CHECK(Draw.ClusterJobs() == 2 && Draw.ClusterBatchRows() == 2 &&
                  Draw.Resident().RecordedCrossings() > 0,
              "failed submission retains staged tables and their pending copies for retry");
      }
      fault.Armed = false;
    }
    CHECK(Draw.HandTables(Error) && Accepted(Draw.Owned().SubmitPendingUploads(), Error),
          "retry submits the complete tables without another scene mutation");
    CHECK(Draw.ClusterJobs() == 2 && Draw.ClusterBatchRows() == 2,
          "both instance jobs remain present after recovery");
    const std::vector<uint32_t> expected{0, 0, 0, 3, 0, 1, 0, 3};
    CHECK(Read(Device, Draw.Resident().Buffer(SubjectResidency::Stream::ClusterJobs).get(), 8) ==
              expected,
          "submitted GPU jobs share the source sphere and retain distinct instance batches");
  }
};

void Tables(Native::Device *device) {
  SubjectDraw draw;
  outshine::Render::Gpu gpu{.Device = device,
                            .HdrFormat = Native::Format::Rgba16Float,
                            .SurfaceFormat = Native::Format::Rgba8Unorm,
                            .Width = 32,
                            .Height = 32};
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "one scene attachment is declared");
  std::string error;
  const bool configured = Accepted(draw.Configure(gpu), error);
  CHECK(configured, error.c_str());
  if (!configured) { return; }
  const uint32_t pipelines = draw.PipelineCount();
  Arm(Failure::Pipeline);
  error.clear();
  CHECK(!Accepted(draw.Configure(gpu), error),
        "a refused subject pipeline rejects reconfiguration");
  CHECK(error.contains("injected"), "a refused subject pipeline retains its native diagnostic");
  CHECK(draw.PipelineCount() == pipelines,
        "a refused subject pipeline preserves the previously published pipeline set");
  fault.Armed = false;
  CHECK(Accepted(draw.Configure(gpu), error), "subject pipelines configure again after a refusal");
  CHECK(draw.PipelineCount() == pipelines,
        "successful retry publishes the complete subject pipeline set");
  const unsigned beforeGroundFailure = fault.Rejected;
  Arm(Failure::Pipeline);
  fault.Skip = pipelines;
  error.clear();
  CHECK(!Accepted(draw.Configure(gpu), error), "a refused ground pipeline rejects reconfiguration");
  CHECK(fault.Rejected == beforeGroundFailure + 1 && error.contains("injected"),
        "a refused ground pipeline reports its injected native diagnostic");
  CHECK(draw.PipelineCount() == pipelines,
        "a refused ground pipeline preserves the previously published subject pipelines");
  fault.Armed = false;
  CHECK(Accepted(draw.Configure(gpu), error),
        "the ground pipeline configures again after a refusal");
  const std::array<SubjectMaterial, 1> materials{};
  const bool materialReady = draw.SetMaterials(materials, error);
  CHECK(materialReady, error.c_str());
  if (!materialReady) { return; }
  const std::array<uint32_t, 1> slots{0};
  draw.SetNativePieceSurfaces(slots);
  const std::array<StoredVertex, 3> vertices{StoredVertex::Of({{-1, -1, 0}}, {}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, -1, 0}}, {}, {{0, 0, 1}}),
                                             StoredVertex::Of({{0, 1, 0}}, {}, {{0, 0, 1}})};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  const std::array<DagCluster, 1> clusters{{{.SelfRadius = 2, .ParentRadius = 2, .Count = 3}}};
  const PieceMesh piece{.Verts = vertices, .Indices = indices, .Clusters = clusters};
  const auto id = draw.PlacePiece(piece, error);
  CHECK(id != kNoPiece && draw.HandTables(error) &&
            Accepted(draw.Owned().SubmitPendingUploads(), error),
        "initial clustered piece publishes tables");
  if (id == kNoPiece) { return; }
  CHECK(draw.ClusterJobs() == 1, "fixture starts with one actual clustered job");
  TableRetry retry{.Draw = draw, .Device = device, .Piece = id, .Row = piece.Row, .Error = error};
  for (const Failure point : {Failure::Map, Failure::Unmap}) {
    for (const unsigned after : {0u, 1u}) { retry.Check(point, after, TablePhase::Staging); }
  }
  retry.Check(Failure::Acquire, 0, TablePhase::Submission);
  retry.Check(Failure::Copy, 0, TablePhase::Submission);
  retry.Check(Failure::Copy, 1, TablePhase::Submission);
  retry.Check(Failure::Submit, 0, TablePhase::Submission);
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"),
        "SDL assertions terminate instead of opening dialogs");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
#ifdef OUTSHINE_GPU_VALIDATION
  constexpr bool debug = true;
#else
  constexpr bool debug = false;
#endif
  {
    auto owned =
        outshine::Test::Gpu::NativeUploadFaultDevice::Create(&fault, {.Validation = debug});
    CHECK(owned.has_value(), "real native GPU device is available");
    if (owned) {
      outshine::Test::Gpu::NativeOfflineDevice device(**owned);
      Tables(&device);
      Growth(&device);
      DeferredRetry(&device);
      MixedUploads(&device);
      FailedReplacement(&device);
      FailedBatchReplacement(&device);
    }
  }
  SDL_Quit();
  return Report();
}
