#ifndef OUTSHINE_TEST_STRUCTUREPREPARATION_PREPARATIONFIXTURE_H
#define OUTSHINE_TEST_STRUCTUREPREPARATION_PREPARATIONFIXTURE_H

#include "Check.h"
#include "OsmStructurePreparation.h"
#include "OsmXmlReader.h"
#include <chrono>
#include <memory>
#include <semaphore>
#include <utility>

namespace PreparationFixture {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr auto kDeadline = std::chrono::seconds(3);
constexpr int kHeightZoom = 10;

inline std::shared_ptr<const SourceSnapshot> Source() {
  auto parsed =
      XmlReader::Read("<osm version='0.6'><node id='1' lat='0.01' lon='0.01'/>"
                      "<node id='2' lat='0.01' lon='0.011'/><node id='3' lat='0.011' lon='0.011'/>"
                      "<node id='4' lat='0.011' lon='0.01'/><way id='10'><nd ref='1'/><nd ref='2'/>"
                      "<nd ref='3'/><nd ref='4'/><nd ref='1'/><tag k='building' v='yes'/>"
                      "<tag k='building:levels' v='3'/></way></osm>",
                      {.DatasetId = "structure-preparation", .Revision = "r1"});
  CHECK(parsed, "the original building input parses");
  if (!parsed) { return {}; }
  return std::make_shared<const SourceSnapshot>(SourceSnapshot{
      .Elements = std::move(*parsed),
      .Coverage = {{.WestDeg = 0.0, .SouthDeg = 0.0, .EastDeg = 0.02, .NorthDeg = 0.02}},
      .Chunks = {}});
}

class WorkerBlock {
public:
  WorkerBlock() = default;

  ~WorkerBlock() { Release(); }

  WorkerBlock(const WorkerBlock &) = delete;
  WorkerBlock &operator=(const WorkerBlock &) = delete;

  [[nodiscard]] bool Start(Tasks &pool) {
    Signal_ = std::make_shared<Signal>();
    if (!pool.PostDetached([signal = Signal_] {
          signal->Entered.release();
          signal->Release.acquire();
        })) {
      Signal_.reset();
      return false;
    }
    return Signal_->Entered.try_acquire_for(kDeadline);
  }

  void Release() {
    if (const auto signal = std::exchange(Signal_, {})) { signal->Release.release(); }
  }

private:
  struct Signal {
    std::binary_semaphore Entered{0}, Release{0};
  };

  std::shared_ptr<Signal> Signal_;
};

inline void Barrier(Tasks &pool) {
  const auto handle = pool.Post([] {});
  CHECK(handle != Tasks::kNoTask, "the pool accepts a terminal barrier");
  if (handle != Tasks::kNoTask) { pool.Wait(handle); }
}
}
#endif
