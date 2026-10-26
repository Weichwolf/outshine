#ifndef OUTSHINE_TEST_DELAYEDTERRAINSOURCE_H
#define OUTSHINE_TEST_DELAYEDTERRAINSOURCE_H

#include "SourceSet.h"
#include "ReadTextFile.h"
#include "Check.h"
#include <cstdint>
#include <vector>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace outshine::Test {
class DelayedTerrainSource final : public outshine::Data::Source {
public:
  enum class Answer { Height, Absent, Refused };
  static constexpr float HeightM = 42.0f;
  outshine::Data::SourceDecl Decl;
  std::vector<uint8_t> Payload;

  DelayedTerrainSource() {
    Decl.Id = "delayed";
    Decl.Revision = "r1";
    Decl.Keeps = Data::Cacheability::Never;
    const auto bytes = ReadTextFile("test/outshine/data/terrain/constant-42.png", 4096);
    CHECK(bytes.has_value(), "constant terrain raster fixture loads");
    if (bytes) { Payload.assign(bytes->begin(), bytes->end()); }
  }

  std::thread::id Caller = std::this_thread::get_id();
  mutable std::atomic<bool> CalledOnCaller{false};
  mutable std::atomic<int> Collections{0};
  std::atomic<bool> Released{false};
  Answer Response = Answer::Height;

  const outshine::Data::SourceDecl &Declaration() const noexcept override { return Decl; }

  outshine::Data::Coverage
  Covers([[maybe_unused]] const outshine::Data::SourceRequest &request) const noexcept override {
    return outshine::Data::Coverage::Inside;
  }

  outshine::Data::Address
  Serves(const outshine::Data::SourceRequest &request) const noexcept override {
    return request.Where();
  }

  outshine::Data::FetchStart
  Begin([[maybe_unused]] const outshine::Data::Address &at,
        [[maybe_unused]] outshine::Data::SourceTransport &transport) const override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::SourceResponse
  Collect([[maybe_unused]] const outshine::Data::Address &at,
          [[maybe_unused]] outshine::Data::Ticket ticket,
          [[maybe_unused]] outshine::Data::SourceTransport &transport) const override {
    ++Collections;
    if (std::this_thread::get_id() == Caller) { CalledOnCaller = true; }
    if (!Released) { return outshine::Data::SourceResponse::Working(); }
    if (Response == Answer::Absent) {
      return outshine::Data::SourceResponse::Meant(outshine::Data::Meaning::Absent);
    }
    if (Response == Answer::Refused) {
      return outshine::Data::SourceResponse::Meant(outshine::Data::Meaning::Refused);
    }
    return outshine::Data::SourceResponse::Delivered(Payload);
  }
};

class NoNetwork final : public outshine::Data::SourceTransport {
public:
  outshine::Data::FetchStart Begin([[maybe_unused]] const std::string &url) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect([[maybe_unused]] outshine::Data::Ticket ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel([[maybe_unused]] outshine::Data::Ticket ticket) override {}
};
}

#endif
