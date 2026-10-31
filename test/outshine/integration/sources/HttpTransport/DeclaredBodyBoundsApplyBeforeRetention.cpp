#include "Check.h"
#include "HttpTransport.h"
#include "LoopbackHttpServer.h"
#include "SourceTransport.h"
#include <chrono>
#include <cstddef>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Data;

std::string Respond(const std::string & /*request*/) {
  return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
         "3\r\naaa\r\n7\r\naaaaaaa\r\nb\r\naaaaaaaaaaa\r\n9\r\naaaaaaaaa\r\n0\r\n\r\n";
}

Wire Await(HttpTransport &wire, Ticket ticket) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    auto response = wire.Collect(ticket);
    if (response.Where() != Wire::State::Working) { return response; }
    (void)wire.Await(1);
  }
  wire.Cancel(ticket);
  return Wire::Unreachable(SourceFailureReason::TimedOut);
}
}

int main() {
  using namespace outshine::Test;
  const LoopbackHttpServer server(Respond);
  CHECK(server.Ready(), "a real local chunked HTTP endpoint opens");
  if (!server.Ready()) { return Report(); }
  HttpTransport wire({.ConcurrentTransfers = 1});
  constexpr size_t most = 30;
  const auto exact = wire.Begin(server.Url(), most);
  CHECK(exact, "a bounded request enters the actual HTTP worker");
  if (exact) {
    const auto response = Await(wire, *exact).Take();
    CHECK(response && response->Status == 200 && response->Body.size() == most &&
              response->Body.capacity() <= most,
          "chunk growth retains exactly the admitted non-power-of-two body capacity");
  }
  const auto shortRead = wire.Begin(server.Url(), most - 1);
  CHECK(shortRead, "a smaller per-request bound can share the same transport");
  if (shortRead) {
    const auto response = Await(wire, *shortRead);
    CHECK(response.Where() == Wire::State::Unreachable &&
              response.FailureReason() == SourceFailureReason::CapacityRefused,
          "the extra chunk is refused before a larger buffer is retained");
  }
  CHECK(!wire.Begin(server.Url(), size_t{0}), "a zero body bound is rejected before IO");
  CHECK(server.Requests().size() == 2, "only admitted requests reach the local server");
  return Report();
}
