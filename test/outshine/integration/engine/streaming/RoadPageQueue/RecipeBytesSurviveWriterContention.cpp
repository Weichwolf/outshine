#include "test/harness/engine/RoadPublicationContention.h"
#include "RoadPageMeshProducer.h"
#include "RoadPageRecipeCodec.h"
#include "Check.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using namespace outshine::Test::RoadPublication;

RoadPageRecipe Recipe() {
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = 7,
                              .Lane = 0,
                              .Stations = {{.EastM = 0}, {.EastM = 96}},
                              .Surface = {.HalfWidthM = 3,
                                          .Profile = RoadProfile::Simple,
                                          .WearsLinear = {{.2f, .3f, .4f}},
                                          .Form = RibbonForm::Surface}});
  return recipe;
}

void CheckRetry() {
  Fixture fixture;
  CHECK(fixture.Open(), "source and display publication use a private native writer");
  if (!fixture.Cache) { return; }
  const auto request = Request();
  const RoadPageMeshProducer producer(RoadPageKey(request.Address), Recipe());
  auto write = Pages::PreparePage(request);
  CHECK(write, "a complete source request owns its publication continuation");
  if (!write) { return; }
  fixture.Hold();
  const auto pending = fixture.Cache->Prepare(*write, &producer);
  CHECK(!pending && Busy(pending.error()) && write->Costs().Preparations == 1 &&
            write->Costs().InputPublications == 1 && !write->NeedsProducer(),
        "the first source Busy retains both encoded products after one generation");
  const size_t bytes = write->PublicationBytes();
  const size_t reads = write->Costs().MetadataReads;
  CHECK(bytes > 0, "input and render package ownership remains charged during writer contention");
  for (size_t at = 0; at < 3; ++at) {
    const auto busy = fixture.Cache->Prepare(*write);
    CHECK(!busy && Busy(busy.error()) && write->PublicationBytes() == bytes &&
              write->Costs().Preparations == 1 && write->Costs().MetadataReads == reads,
          "later source publication attempts need no producer, serialization or metadata refresh");
  }
  fixture.Release();
  const auto ready = fixture.Cache->Prepare(*write);
  const auto input = fixture.Cache->PlanRecipeRead(request.Address, kBytesMost);
  const auto display = fixture.Cache->Load(request.Address, kBytesMost);
  CHECK(ready && input && *input && display && *display && write->PublicationBytes() == 0,
        "source and display become independently readable after the writer releases");
}

void CheckCancel() {
  Fixture fixture;
  CHECK(fixture.Open(), "cancellation owns another isolated native source store");
  if (!fixture.Cache) { return; }
  const auto request = Request();
  const RoadPageMeshProducer producer(RoadPageKey(request.Address), Recipe());
  auto write = Pages::PreparePage(request);
  if (!write) {
    CHECK(false, "the cancelable source request validates");
    return;
  }
  fixture.Hold();
  const auto pending = fixture.Cache->Prepare(*write, &producer);
  CHECK(!pending && Busy(pending.error()), "source cancellation starts from a real writer wait");
  std::stop_source stop;
  (void)stop.request_stop();
  const auto canceled = fixture.Cache->Prepare(*write, nullptr, stop.get_token());
  CHECK(!canceled && !Busy(canceled.error()), "stopped source publication is terminal");
  write = std::unexpected(std::string("discarded canceled source request"));
  fixture.Release();
  const auto input = fixture.Cache->PlanRecipeRead(request.Address, kBytesMost);
  const auto display = fixture.Cache->Load(request.Address, kBytesMost);
  CHECK(input && !*input && display && !*display,
        "neither retained package can publish after its canceled owner is released");
}
}

int main() {
  CheckRetry();
  CheckCancel();
  return Report();
}
