#ifndef OUTSHINE_WORLD_DATA_OFFLINETRANSPORT_H
#define OUTSHINE_WORLD_DATA_OFFLINETRANSPORT_H

#include <world/data/Transport.h>
#include <expected>

namespace outshine::Data {

class OfflineTransport final : public Transport {
public:
  [[nodiscard]] FetchStart Begin(const std::string &url) override {
    (void)url;
    return std::unexpected(FetchFailureReason::OfflineMiss);
  }

  [[nodiscard]] FetchStart Begin([[maybe_unused]] const std::string &url,
                                 [[maybe_unused]] ByteRange range,
                                 [[maybe_unused]] std::string_view entityTag = {}) override {
    return std::unexpected(FetchFailureReason::OfflineMiss);
  }

  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Never(FetchFailureReason::OfflineMiss);
  }

  void Cancel(Ticket ticket) override { (void)ticket; }

  [[nodiscard]] bool Await(double waitMs) override {
    (void)waitMs;
    return false;
  }
};

}

#endif
