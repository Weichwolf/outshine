#include "Check.h"
#include "Fetching.h"
#include "LoopbackHttpServer.h"

#include <array>
#include <chrono>
#include <csignal>
#include <optional>
#include <string>

namespace {
using Clock = std::chrono::steady_clock;
using outshine::Data::Wire;

std::optional<Wire::Response> Receive(outshine::Fetching &transport,
                                      outshine::Data::Ticket ticket,
                                      std::chrono::milliseconds budget) {
  const auto deadline = Clock::now() + budget;
  while (Clock::now() < deadline) {
    auto reply = transport.Collect(ticket);
    if (auto response = reply.Take()) { return response; }
    if (reply.Where() != Wire::State::Working) { return {}; }
    (void)transport.Await(10);
  }
  return {};
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace std::chrono_literals;
  std::signal(SIGPIPE, SIG_IGN);
  for (const int status : std::array{429, 509}) {
    LoopbackHttpServer limited([status](const std::string &request) {
      if (request.starts_with("GET /limit ")) {
        return "HTTP/1.1 " + std::to_string(status) +
               " Limited\r\nRetry-After: 1\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      }
      return std::string("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
    });
    LoopbackHttpServer other([](const std::string &) {
      return std::string("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
    });
    CHECK(limited.Ready() && other.Ready(), "independent loopback origins are available");
    if (!limited.Ready() || !other.Ready()) { continue; }
    Fetching transport({.ConcurrentTransfers = 1, .ConnectionsPerHost = 1, .TimeoutS = 2});
    const auto first = transport.Begin(limited.Url("LOCALHOST") + "/limit");
    CHECK(first.has_value(), "first quota request admitted");
    if (!first) { continue; }
    const auto quota = Receive(transport, *first, 2s);
    CHECK(quota && quota->Status == status, "quota response delivered to its original consumer");
    if (!quota) { continue; }
    auto sameUrl = limited.Url();
    sameUrl.insert(sameUrl.rfind(':') + 1, "0");
    const auto same = transport.Begin(sameUrl + "/same");
    const auto canceled = transport.Begin(limited.Url() + "/cancel");
    const auto independent = transport.Begin(other.Url() + "/other");
    CHECK(same && canceled && independent, "bounded queue accepts delayed and independent work");
    if (!same || !canceled || !independent) { continue; }
    transport.Cancel(*canceled);
    CHECK(transport.Collect(*canceled).FailureReason() == Data::FetchFailureReason::Cancelled,
          "origin-blocked requests remain cancelable");
    const auto ready = Receive(transport, *independent, 700ms);
    CHECK(ready && ready->Status == 200,
          "a delayed queue head does not block a different port on the same host");
    const auto resumed = Receive(transport, *same, 3s);
    CHECK(resumed && resumed->Status == 200, "queued work resumes when its origin cooldown ends");
    const auto requests = limited.Requests();
    CHECK(requests.size() == 2 && requests[1].Text.starts_with("GET /same "),
          "only the intended requests reached the rate-limited origin");
    CHECK(requests.size() == 2 && requests[1].At - requests[0].At >= 1s,
          "every path and hostname spelling shares the full server cooldown");
  }
  return Report();
}
