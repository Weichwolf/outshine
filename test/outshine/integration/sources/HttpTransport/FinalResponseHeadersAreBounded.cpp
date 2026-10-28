#include "Check.h"
#include "HttpTransport.h"
#include "LoopbackHttpServer.h"
#include "SourceTransport.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Test;
using Clock = std::chrono::steady_clock;

std::string Respond(const std::string &request) {
  if (request.starts_with("GET /redirect ")) {
    return "HTTP/1.1 302 Found\r\nLocation: /final\r\nContent-Length: 3\r\n"
           "X-Identity: stale\r\nConnection: close\r\n\r\nOLD";
  }
  if (request.starts_with("GET /large ")) {
    return "HTTP/1.1 200 OK\r\nX-Padding: " + std::string(MaximumResponseHeaderBytes + 1, 'x') +
           "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  }
  std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nX-Identity: final\r\n"
                         "Connection: close\r\n\r\nNEW";
  if (request.starts_with("GET /interim ")) {
    response.insert(0, "HTTP/1.1 100 Continue\r\nX-Identity: interim\r\n\r\n");
  }
  return response;
}

Wire Await(HttpTransport &wire, Ticket ticket) {
  const auto deadline = Clock::now() + std::chrono::seconds(3);
  while (Clock::now() < deadline) {
    auto reply = wire.Collect(ticket);
    if (reply.Where() != Wire::State::Working) { return reply; }
    (void)wire.Await(5);
  }
  wire.Cancel(ticket);
  return Wire::Unreachable(SourceFailureReason::TimedOut);
}
}

int main() {
  using namespace outshine::Test;
  const LoopbackHttpServer server(Respond);
  CHECK(server.Ready(), "the real loopback HTTP server starts");
  if (!server.Ready()) { return Report(); }
  HttpTransport wire({.ConcurrentTransfers = 1});
  for (const auto *const path : {"/final", "/redirect", "/interim"}) {
    const auto ticket = wire.Begin(server.Url() + path);
    CHECK(ticket, "an actual non-range request starts");
    if (!ticket) { continue; }
    const auto response = Await(wire, *ticket).Take();
    CHECK(response && response->Status == 200 &&
              response->Body == std::vector<uint8_t>({'N', 'E', 'W'}) &&
              SingleResponseHeader(response->Headers, "x-identity") == "final",
          "only the final response's metadata and body are delivered together");
  }
  const auto large = wire.Begin(server.Url() + "/large");
  CHECK(large, "a response with an oversized header starts within the existing queue budget");
  if (large) {
    const auto reply = Await(wire, *large);
    CHECK(reply.Where() == Wire::State::Unreachable &&
              reply.FailureReason() == SourceFailureReason::CapacityRefused,
          "header overflow is a bounded capacity refusal rather than a delivered payload");
  }
  return Report();
}
