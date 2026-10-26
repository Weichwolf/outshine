#ifndef OUTSHINE_SOURCES_OFFLINESOURCETRANSPORT_H
#define OUTSHINE_SOURCES_OFFLINESOURCETRANSPORT_H

#include <SourceTransport.h>
#include <expected>

namespace outshine::Data {

class OfflineSourceTransport final : public SourceTransport {
public:
  [[nodiscard]] FetchStart Begin(const std::string &url) override {
    (void)url;
    return std::unexpected(SourceFailureReason::OfflineMiss);
  }

  [[nodiscard]] FetchStart Begin([[maybe_unused]] const std::string &url,
                                 [[maybe_unused]] ByteRange range,
                                 [[maybe_unused]] std::string_view entityTag = {}) override {
    return std::unexpected(SourceFailureReason::OfflineMiss);
  }

  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Never(SourceFailureReason::OfflineMiss);
  }

  void Cancel(Ticket ticket) override { (void)ticket; }

  [[nodiscard]] bool Await(double waitMs) override {
    (void)waitMs;
    return false;
  }
};

}

#endif
