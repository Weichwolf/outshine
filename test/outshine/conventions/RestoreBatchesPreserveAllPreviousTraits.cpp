#include "Column.h"
#include "Traits.h"
#include "Outshine.h"
#include "io/ReadTextFile.h"
#include "Check.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  EntityRegistry registry;
  CHECK(registry.open(4), "registry opened");
  const auto first = registry.addEntity(Role::Tool);
  const auto second = registry.addEntity(Role::Tool);
  Column<int> values;
  CHECK(values.Open(registry) && values.Put(first, 10) && values.Put(second, 20),
        "columns prepared");
  std::array<Column<int>::Replacement, 2> changed{{{first, 100}, {kNoEntity, 200}}};
  CHECK(!values.Replace(changed) && *values.Get(first) == 10,
        "invalid late target preserves an earlier valid target");
  changed[1].Owner = second;
  CHECK(values.Replace(changed) && *values.Get(first) == 100 && *values.Get(second) == 200,
        "validated batch replaces all existing components");
  registry.remove(second);
  changed[0].Data = 999;
  CHECK(!values.Replace(changed) && *values.Get(first) == 100,
        "stale late target rejects before any publication");

  auto path = (std::filesystem::temp_directory_path() / "outshine-restore-XXXXXX").string();
  const int descriptor = mkstemp(path.data());
  if (descriptor < 0) {
    CHECK(false, "temporary save created");
    return Report();
  }
  close(descriptor);
  Scenario::Document scene;
  scene.Named.Name = "restore";
  scene.Room = 4;
  Scenario::Kind kind;
  kind.Name = "actor";
  kind.Attributes = {{"health", "23"}};
  scene.Kinds.push_back(kind);
  scene.Instances = {{.Of = "actor", .Id = "first"}, {.Of = "actor", .Id = "second"}};
  scene.State = {{.What = "first.health"}, {.What = "second.health"}};
  Engine engine;
  const bool ready = engine.declare(scene).has_value() && engine.assemble().has_value() &&
                     engine.save(path).has_value();
  CHECK(ready, "public simulation assembled and saved");
  if (!ready) {
    std::filesystem::remove(path);
    return Report();
  }
  const auto initial = ReadTextFile(path, 4096);
  CHECK(initial.has_value(), "initial save read");
  if (!initial) {
    std::filesystem::remove(path);
    return Report();
  }
  const auto header = initial->substr(0, initial->find('\n') + 1);
  for (const std::string line :
       {"second.health nan\n", "second.absent 5\n", "missing.health 5\n"}) {
    {
      std::ofstream output(path);
      output << header << "first.health 99\n" << line;
    }
    CHECK(!engine.restore(path), "invalid late row rejects restore");
    CHECK(engine.save(path).has_value(), "current state remains serializable");
    const auto preserved = ReadTextFile(path, 4096);
    CHECK(preserved && *preserved == *initial, "failed restore preserves every prior trait");
  }
  {
    std::ofstream output(path);
    output << header << "second.health 8\nfirst.health 9\nsecond.health 11\n";
  }
  CHECK(engine.restore(path).has_value() && engine.save(path).has_value(),
        "valid grouped restore succeeds");
  const auto final = ReadTextFile(path, 4096);
  CHECK(final && *final == header + "first.health 9\nsecond.health 11\n",
        "grouping preserves last value in file order for repeated traits");
  std::filesystem::remove(path);
  return Report();
}
