#include "Corridors.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
bool Corridors::UsesWay(const Paving &on, size_t way) {
  return !on.FunctionalPlan || std::ranges::binary_search(on.FunctionalWays, way);
}

bool Corridors::MatchesFunctionalInput(const Job &job, const Site &site) {
  return !job.Functional ||
         (site.Vectors != nullptr && site.Vectors->OriginToken() == job.Functional->Origin.get() &&
          site.Standing.Anchor() == job.Functional->Anchor);
}

std::expected<std::unique_ptr<Corridors::Job>, std::string_view>
Corridors::BeginFunctionalPlan(const Site &site, std::span<const uint32_t> ways) {
  if (site.Vectors == nullptr) {
    return std::unexpected("functional transport planning requires its source snapshot");
  }
  for (const auto index : ways) {
    if (!site.Ways.SourceOf(*site.Vectors, index)) {
      return std::unexpected("functional transport demand does not match its source ways");
    }
  }
  auto job = Begin(site);
  Job::FunctionalDemand demand{.Ways = {ways.begin(), ways.end()},
                               .Origin = site.Vectors->ShareOriginToken(),
                               .Anchor = site.Standing.Anchor()};
  std::ranges::sort(demand.Ways);
  const auto duplicates = std::ranges::unique(demand.Ways);
  demand.Ways.erase(duplicates.begin(), duplicates.end());
  job->Functional = std::move(demand);
  return job;
}
}
