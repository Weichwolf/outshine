#include "Check.h"
#include "ReadbackTransfer.h"
#include "SubjectResidency.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <chrono>
#include <utility>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Render::Native;
using namespace outshine::Test::Gpu;

namespace {
constexpr std::array<uint32_t, 4> kInitialValues{17, 23, 31, 47};

void Exercise(Device *device) {
  SubjectResidency residency;
  Require(residency.StandsOn(device));
  auto values = kInitialValues;
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  std::array<SubjectResidency::Crossing, 1> crossing{{{.Which = stream,
                                                       .Usage = BufferUse::Storage,
                                                       .From = values.data(),
                                                       .Bytes = sizeof(values)}}};
  for (const bool deferred : {true, false, true}) {
    CHECK(residency.Cross(crossing, deferred), "native data uploads after scratch release");
    const auto capacity = residency.Allocations().TransferBytes;
    CHECK(capacity > 0, "upload allocates scratch storage");
    if (deferred) {
      residency.ReleaseUploadScratch();
      CHECK(residency.Allocations().TransferBytes == capacity,
            "unsubmitted crossings retain their upload storage");
      CHECK(residency.SubmitPendingUploads(), "pending crossings survive the release attempt");
    }
    auto commands = Require(RetryOffline([&] { return device->Begin(); }, std::chrono::seconds(5)));
    const auto complete = Require(device->Submit(std::move(commands)));
    CHECK(Require(device->Wait(complete, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "GPU completes submitted uploads within the explicit offline wait budget");
    auto *const buffer = residency.Buffer(stream).get();
    const auto resident = residency.Allocations().StreamBytes;
    residency.ReleaseUploadScratch();
    CHECK(residency.Allocations().TransferBytes == 0 &&
              residency.Allocations().StreamBytes == resident &&
              residency.Buffer(stream).get() == buffer,
          "release removes scratch without replacing resident buffers");
    auto transfer = Require(ReadbackTransfer::Start(*device, *buffer, {.Bytes = sizeof(values)}));
    const auto read = Require(transfer.Wait(*device, std::chrono::seconds(5)));
    CHECK(read, "resident data remains readable after release");
    CHECK(read && std::memcmp(read->data(), values.data(), sizeof(values)) == 0,
          "resident bytes match the last successful upload");
    ++values[0];
  }
}
}

int main() {
  {
    auto device = Require(CreateDevice({.Validation = true}));
    if (device) { Exercise(device.get()); }
  }
  return Report();
}
