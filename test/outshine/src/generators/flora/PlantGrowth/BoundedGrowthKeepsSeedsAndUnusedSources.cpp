#include "PlantGrowth.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array<PlantGrowthPoint, 2> kSeed{PlantGrowthPoint{0, 0, 0}, {0, 0, 1}};
  constexpr std::array<int32_t, 2> kParents{-1, 0};
  constexpr std::array<PlantGrowthPoint, 1> kDistant{PlantGrowthPoint{0, 0, 5}};
  constexpr double kStepM = 0.1;
  constexpr double kFinalKillM = 0.15;
  PlantGrowthDemand demand{
      .Nodes = 16, .Iterations = 1, .StepM = kStepM, .InfluenceM = 10, .KillM = kStepM};
  const auto partial = GrowPlant(kDistant, kSeed, kParents, demand);
  CHECK(partial && partial->Stop == PlantGrowthStop::IterationBudget &&
            partial->PositionsM.size() == 3 && partial->Unreached.size() == 1 &&
            partial->Unreached[0] == 0 && partial->PositionsM[0] == kSeed[0] &&
            partial->PositionsM[1] == kSeed[1] && partial->Parents[2] == 1,
        "bounded partial growth preserves exact seed prefix, parent IDs and unused source IDs");
  demand.Nodes = 2;
  const auto full = GrowPlant(kDistant, kSeed, kParents, demand);
  CHECK(full && full->Stop == PlantGrowthStop::NodeBudget && full->PositionsM.size() == 2,
        "node capacity ends with the valid supplied seed");
  demand.Nodes = 64;
  constexpr std::array<PlantGrowthPoint, 1> kFinal{PlantGrowthPoint{0, 0, 1.2}};
  demand.KillM = kFinalKillM;
  const auto consumed = GrowPlant(kFinal, kSeed, kParents, demand);
  CHECK(consumed && consumed->Stop == PlantGrowthStop::ConsumedAttractors &&
            consumed->Unreached.empty() && consumed->Stats.Consumed == 1 &&
            consumed->PositionsM.size() == 3,
        "last iteration's new node consumes sources before the finished plan is published");
  const auto empty = GrowPlant({}, kSeed, kParents, demand);
  CHECK(empty && empty->Stop == PlantGrowthStop::ConsumedAttractors &&
            empty->PositionsM.size() == 2,
        "empty source demand preserves a valid seed without invented growth");
  constexpr std::array<PlantGrowthPoint, 2> kOpposed{PlantGrowthPoint{-1, 0, 1}, {1, 0, 1}};
  const auto cancelled = GrowPlant(kOpposed, kSeed, kParents, demand);
  CHECK(cancelled && cancelled->Stop == PlantGrowthStop::CancelledDirections &&
            cancelled->Unreached.size() == 2,
        "opposed source directions terminate without a fabricated arbitrary branch");
  const auto bytes = MeasurePlantGrowth(kDistant, kSeed, kParents, demand);
  CHECK(bytes && partial->Stats.WorkingBytes == size_t{16} * 89 + sizeof(uint32_t),
        "workspace covers both indices, all bounded scratch arrays and retained product arrays");
  demand.WorkingBytes = *bytes - 1;
  CHECK(!GrowPlant(kDistant, kSeed, kParents, demand),
        "insufficient workspace refuses the whole request");
  demand.WorkingBytes = std::numeric_limits<size_t>::max();
  demand.Nodes = std::numeric_limits<size_t>::max();
  CHECK(!MeasurePlantGrowth(kDistant, kSeed, kParents, demand),
        "unrepresentable node IDs fail before arithmetic overflow");
  demand.Nodes = 64;
  constexpr std::array<int32_t, 2> kCyclic{-1, 1};
  CHECK(!GrowPlant(kDistant, kSeed, kCyclic, demand),
        "cyclic seeds cannot enter index construction");
  auto invalid = kSeed;
  invalid[1][0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!GrowPlant(kDistant, invalid, kParents, demand),
        "nonfinite coordinates fail at the source boundary");
  invalid = kSeed;
  invalid[1] = invalid[0];
  CHECK(!GrowPlant(kDistant, invalid, kParents, demand), "collapsed supplied edges are rejected");
  return Report();
}
