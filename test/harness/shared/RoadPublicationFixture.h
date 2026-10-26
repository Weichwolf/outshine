#ifndef OUTSHINE_TEST_ROADPUBLICATIONFIXTURE_H
#define OUTSHINE_TEST_ROADPUBLICATIONFIXTURE_H
#include "AssetPipeline.h"
#include "RoadPageQueue.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Test {
inline std::shared_ptr<AssetPipeline> RoadPublications(Tasks &pool) {
  constexpr size_t kMebibyte = size_t{1024} * 1024;
  return std::make_shared<AssetPipeline>(
      pool,
      AssetPipeline::Limits{.JobsMost = 4,
                            .ConsumersMost = 16,
                            .TicketsMost = 16,
                            .InputBytesMost = 64 * kMebibyte,
                            .ReceiptBytesMost = size_t{16} * 4096});
}

inline bool AdmitRoadPage(RoadPageQueue &queue,
                          Generators::RoadPageRequest request,
                          std::shared_ptr<const Generators::RoadPageProducer> producer = {}) {
  const auto admitted = queue.RequestRecord(std::move(request), std::move(producer));
  return admitted && *admitted;
}

inline Generators::RoadPageLease *ConsumeRoadPage(std::vector<RoadPageQueue::Completed> &done,
                                                  Generators::PreparedRoadPages &pages) {
  if (done.size() != 1) { return nullptr; }
  auto &result = done.front().Result;
  if (!result || !*result) { return nullptr; }
  auto &product = **result;
  if (const auto *receipt = std::get_if<RoadPageQueue::Receipt>(&product);
      receipt != nullptr && *receipt) {
    auto loaded = pages.LoadKey((*receipt)->Key, (*receipt)->ByteCount);
    if (!loaded || !*loaded) { return nullptr; }
    product = std::move(**loaded);
  }
  return std::get_if<Generators::RoadPageLease>(&product);
}
}
#endif
