#include <Logging.h>
#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

class RefusingTransport final : public outshine::Data::Transport {
public:
  outshine::Data::Ticket Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};

class RefusingSource final : public outshine::Data::Source {
public:
  const outshine::Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  outshine::Data::Coverage Covers(const outshine::Data::Fetch &) const noexcept override {
    return outshine::Data::Coverage::Inside;
  }

  outshine::Data::Address Serves(const outshine::Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  outshine::Data::Ticket Begin(const outshine::Data::Address &,
                               outshine::Data::Transport &transport) const override {
    return transport.Begin("fixture");
  }

  outshine::Data::Fetched Collect(const outshine::Data::Address &,
                                  outshine::Data::Ticket,
                                  outshine::Data::Transport &) const override {
    return outshine::Data::Fetched::Meant(outshine::Data::Meaning::Refused);
  }

private:
  outshine::Data::SourceDecl Decl_{
      .Id = "refusing", .Revision = "fixture-1", .Keeps = outshine::Data::Cacheability::Never};
};

class RecordingSink final : public outshine::LogSink {
public:
  void Write(double,
             outshine::LogLevel,
             Saying who,
             std::span<const outshine::LogField> fields) noexcept override {
    const std::scoped_lock lock(Mutex_);
    Events_.emplace_back(who.Event == nullptr ? "" : who.Event);
    if (who.Event != nullptr && std::string_view(who.Event) == "tile_refused") {
      for (const outshine::LogField &field : fields) {
        if (std::string_view(field.Key) == "source") { Source_ = field.Value; }
        if (std::string_view(field.Key) == "revision") { Revision_ = field.Value; }
      }
    }
  }

  size_t Count(std::string_view event) const {
    const std::scoped_lock lock(Mutex_);
    return static_cast<size_t>(std::ranges::count(Events_, event));
  }

  bool Attributed(std::string_view source, std::string_view revision) const {
    const std::scoped_lock lock(Mutex_);
    return Source_ == source && Revision_ == revision;
  }

private:
  mutable std::mutex Mutex_;
  std::vector<std::string> Events_;
  std::string Source_;
  std::string Revision_;
};

}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<RefusingSource>()) == SourceSet::Registration::Accepted,
        "the refusing worker source registers");
  RefusingTransport transport;
  RecordingSink sink;
  {
    TilePool pool({.Threads = 1, .Carriers = 1, .Diagnostics = &sink}, sources, transport);
    TilePool::Landing landing;
    const Fetch request(DataKind::Elevation, Address::Whole(0));
    TilePool::Reply reply = pool.Bytes(request, &landing);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (reply == TilePool::Reply::Pending && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      reply = pool.Bytes(request, &landing);
    }
    CHECK(reply == TilePool::Reply::Refused, "the carrier reports the declared refusal");
    CHECK(sink.Count("tile_refused") == 1,
          "the carrier emits its refusal through the configured sink");
    CHECK(sink.Attributed("refusing", "fixture-1"),
          "refusal identifies the failed source and revision");
  }
  const size_t before = sink.Count("tile_refused");
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  CHECK(sink.Count("tile_refused") == before,
        "destroyed workers cannot call the configured sink later");
  return Report();
}
