#include <array>
#include "src/engine/WorldInstanceSink.h"
#include "src/base/io/Heap.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto region = Generators::Tile::Of(12, {.LongitudeDeg = 9, .LatitudeDeg = 47});
  std::array<WorldInstance, 2> storage{};
  storage[1].Body = 91;
  WorldInstanceSink sink(std::span(storage).first(1), region);
  const Generators::BodyRange bodies{.First = 7, .Count = 2};
  const auto cluster = static_cast<Generators::ClusterId>(13);
  const Generators::Scattered instance{.Em = 10, .Nm = 20, .AslM = 123, .YawRad = 0.5f, .Scale = 2};
  constexpr auto tag = "instance-sink-test";
  const Heap::Tagged tagged(tag);
  const size_t before = Heap::TakenUnder(tag);
  const bool added = sink.Add(bodies.Nth(0), cluster, instance);
  const bool refused = !sink.Add(bodies.Nth(1), cluster, instance);
  const size_t after = Heap::TakenUnder(tag);
  CHECK(Heap::ProcessInstrumentationEnabled(), "allocation oracle is enabled");
  CHECK(added && refused && sink.Full() && sink.Written() == 1,
        "prepared capacity bounds callback writes");
  CHECK(sink.Error() == InstanceWriteError::Capacity, "overflow is an explicit latched failure");
  CHECK(before == after, "successful and rejected callbacks allocate no heap storage");
  CHECK(storage[0].Body == 7 && storage[0].Cluster == 13, "source identity survives conversion");
  CHECK(storage[0].Where.AslM == 123 && storage[0].Where.YawRad == 0.5f &&
            storage[0].Where.Scale == 2,
        "placement values survive conversion");
  CHECK(storage[1].Body == 91, "capacity rejection preserves adjacent storage");
  WorldInstanceSink empty({}, region);
  CHECK(empty.Full() && !empty.Add(bodies.Nth(0), cluster, instance) && empty.Written() == 0,
        "empty output refuses without touching memory");
  return Report();
}
