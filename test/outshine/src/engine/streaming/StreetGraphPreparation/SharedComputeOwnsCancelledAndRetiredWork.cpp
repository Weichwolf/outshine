#include "StreetGraphPreparation.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "StreetField.h"
#include "VegetationTemplates.h"
#include "Check.h"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {
using Clock = std::chrono::steady_clock;

struct ComputeGate {
  std::shared_ptr<std::atomic<bool>> Open = std::make_shared<std::atomic<bool>>(false);
  std::shared_ptr<std::atomic<bool>> Entered = std::make_shared<std::atomic<bool>>(false);

  explicit ComputeGate(outshine::Tasks &pool) {
    (void)pool.PostDetached([open = Open, entered = Entered] {
      entered->store(true);
      const auto deadline = Clock::now() + std::chrono::seconds(5);
      while (!open->load() && Clock::now() < deadline) { std::this_thread::yield(); }
    });
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (!Entered->load() && Clock::now() < deadline) { std::this_thread::yield(); }
  }

  ~ComputeGate() { Open->store(true); }
};

bool Await(outshine::StreetGraphPreparation &work) {
  const auto deadline = Clock::now() + std::chrono::seconds(3);
  while (!work.Complete() && Clock::now() < deadline) { std::this_thread::yield(); }
  return work.Complete();
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            templates.Load("src/assets/world/vegetation.json", materials),
        "existing street rules are available");
  if (!templates.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  ::outshine::Generators::Osm::OsmField field(14, layers);
  const std::array<::outshine::Generators::Osm::OsmField::Declared, 2> roads{
      {{.Layer = "streets", .Key = "kind", .Value = "path", .LatLon = {47, 9, 47, 9.002}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "path",
        .LatLon = {46.999, 9.001, 47.001, 9.001}}}};
  CHECK(field.Declare(roads, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value(),
        "two crossing street inputs are declared");
  ::outshine::Generators::Osm::StreetField streets;
  CHECK(streets.Ingest(field, templates) == 2, "both native streets are ingested");
  const auto heightOf = [](LongitudeLatitude) { return std::optional<double>(100); };
  const auto oracle = StreetGraphBuilder::BuildOneShot(streets, field.Points(), heightOf);
  CHECK(oracle.Graph && oracle.Ways == 2 && oracle.Nodes >= 4 && oracle.Edges >= 4,
        "the preserved synchronous path builds a connected crossing graph");
  if (!oracle.Graph) { return Report(); }
  Tasks pool(1);
  {
    ComputeGate gate(pool);
    CHECK(gate.Entered->load(), "the shared compute worker is deliberately occupied");
    auto job = StreetGraphBuildJob::Begin(field, streets, heightOf);
    CHECK(job.has_value(), "the asynchronous graph job is accepted");
    if (!job) { return Report(); }
    StreetGraphPreparation work(pool, std::move(*job));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(!work.Complete() && work.Advances() == 0,
          "street graph work uses the occupied shared worker rather than a private thread");
    gate.Open->store(true);
    CHECK(Await(work), "the graph completes after its shared compute turn");
    auto completed = work.Collect();
    CHECK(completed && *completed && completed->value().Graph.Graph,
          "completed native graph ownership is delivered");
    if (completed && *completed) {
      const auto &actual = completed->value().Graph;
      CHECK(actual.Ways == oracle.Ways && actual.Nodes == oracle.Nodes &&
                actual.Edges == oracle.Edges && actual.Junctions == oracle.Junctions &&
                actual.Graph->CrossingsJoined() == oracle.Graph->CrossingsJoined(),
            "shared compute preserves the existing one-shot network topology");
    }
    CHECK(!work.Collect(), "one completed graph transfers exactly once");
  }
  {
    ComputeGate gate(pool);
    auto job = StreetGraphBuildJob::Begin(field, streets, heightOf);
    CHECK(job.has_value(), "a queued graph can be canceled");
    if (!job) { return Report(); }
    StreetGraphPreparation work(pool, std::move(*job));
    work.Cancel();
    gate.Open->store(true);
    CHECK(Await(work), "queued cancellation reaches a terminal result");
    auto result = work.Collect();
    CHECK(result && !*result && result->error() == "street graph candidate canceled" &&
              work.Advances() == 0,
          "canceled work does not advance or publish a stale graph");
  }
  {
    ComputeGate gate(pool);
    auto pin = std::make_shared<int>(100);
    const std::weak_ptr<int> retained = pin;
    std::unique_ptr<StreetGraphPreparation> work;
    {
      auto job = StreetGraphBuildJob::Begin(
          field, streets, [pin](LongitudeLatitude) { return std::optional<double>(*pin); });
      CHECK(job.has_value(), "retired graph work owns its height inputs");
      if (!job) { return Report(); }
      work = std::make_unique<StreetGraphPreparation>(pool, std::move(*job));
    }
    pin.reset();
    const auto began = Clock::now();
    work.reset();
    const double retiredMs =
        std::chrono::duration<double, std::milli>(Clock::now() - began).count();
    CHECK(retiredMs < 16.67 && !retained.expired(),
          "retirement does not join the occupied worker and queued inputs remain alive");
    gate.Open->store(true);
    const auto marker = pool.Post([] {});
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    bool drained = false;
    while (!drained && Clock::now() < deadline) { drained = pool.TakeCompletion(marker); }
    CHECK(drained && retained.expired(),
          "the compute queue releases retired input ownership without orphan completion events");
  }
  return Report();
}
