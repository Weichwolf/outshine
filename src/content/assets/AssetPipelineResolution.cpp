#include "AssetPipelineState.h"
#include <algorithm>
#include <expected>
#include <memory>
#include <mutex>
#include <utility>

namespace outshine {
void AssetPipeline::State::Join(const std::shared_ptr<Job> &from, const std::shared_ptr<Job> &to) {
  for (auto &[id, demand] : Tickets) {
    if (demand.Work == from) { demand.Work = to; }
  }
  to->References += std::exchange(from->References, 0);
  to->Demands += std::exchange(from->Demands, 0);
  for (auto &[key, work] : Jobs) {
    if (work == from) { work = to; }
  }
  from->Finished = true;
}

void AssetPipeline::State::RetireJoined(const std::shared_ptr<Job> &job) {
  std::shared_ptr<Reservation> released;
  {
    const std::scoped_lock lock(Lock);
    job->Running = false;
    --Held.Running;
    --Held.Jobs;
    Held.InputBytes -= std::exchange(job->InputBytes, 0);
    released = std::move(job->Output);
  }
}

std::expected<bool, AssetPublicationError>
AssetPipeline::State::Resolve(const std::shared_ptr<Job> &job, const ResolvedWork &resolved) {
  std::shared_ptr<Job> stopped;
  bool joined = false;
  {
    const std::scoped_lock lock(Lock);
    if (job->Domain != KeyDomain::Request || !resolved.Produce) {
      return std::unexpected(AssetPublicationError::InvalidRequest);
    }
    if (job->StopRequested) { return std::unexpected(AssetPublicationError::Cancelled); }
    const ScopedKey content{.Scope = job->Scope, .Domain = KeyDomain::Content, .Key = resolved.Key};
    const auto found = Jobs.find(content);
    if (found != Jobs.end() && !found->second->StopRequested) {
      const auto target = found->second;
      if (job->Output->Bytes < target->Output->Bytes) {
        return std::unexpected(AssetPublicationError::ResourceDeferred);
      }
      Join(job, target);
      joined = true;
    } else {
      if (found != Jobs.end()) { stopped = found->second; }
      Jobs.insert_or_assign(content, job);
      job->Key = resolved.Key;
      job->Domain = KeyDomain::Content;
    }
  }
  if (stopped) { Dispatch(stopped); }
  return joined;
}

void AssetPipeline::State::PruneRequest(const Demand &demand) {
  if (!demand.Address || demand.Work->Domain != KeyDomain::Content) { return; }
  if (std::ranges::any_of(Tickets, [&](const auto &entry) {
        return entry.second.Work == demand.Work && entry.second.Address == demand.Address;
      })) {
    return;
  }
  const ScopedKey key{.Scope = demand.Work->Scope,
                      .Domain = KeyDomain::Request,
                      .Key = AssetProductKey{demand.Address->Digest}};
  const auto found = Jobs.find(key);
  if (found != Jobs.end() && found->second == demand.Work) { Jobs.erase(found); }
}
}
