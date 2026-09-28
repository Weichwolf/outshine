#include "Fetching.h"
#include "Check.h"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <poll.h>
#include <optional>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

class Server {
public:
  enum class ResponseMode { SecondOnly, Always };
  ResponseMode Responses;
  int Listener = socket(AF_INET, SOCK_STREAM, 0);
  uint16_t Port = 0;
  std::atomic<unsigned> Accepted{0};
  std::jthread Worker;

  explicit Server(ResponseMode response = ResponseMode::SecondOnly) : Responses(response) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (Listener < 0 ||
        bind(Listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(Listener, 8) != 0) {
      return;
    }
    socklen_t size = sizeof(address);
    if (getsockname(Listener, reinterpret_cast<sockaddr *>(&address), &size) != 0) { return; }
    Port = ntohs(address.sin_port);
    Worker = std::jthread([this](std::stop_token stop) {
      std::vector<int> stalled;
      while (!stop.stop_requested()) {
        pollfd descriptor{.fd = Listener, .events = POLLIN, .revents = 0};
        if (poll(&descriptor, 1, 20) <= 0) { continue; }
        const int client = accept(Listener, nullptr, nullptr);
        if (client < 0) { continue; }
        const unsigned count = ++Accepted;
        if (count == 2 || Responses == ResponseMode::Always) {
          pollfd request{.fd = client, .events = POLLIN, .revents = 0};
          char headers[4096];
          if (poll(&request, 1, 1000) > 0) { (void)recv(client, headers, sizeof(headers), 0); }
          constexpr char response[] =
              "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK";
          (void)send(client, response, sizeof(response) - 1, 0);
          close(client);
        } else {
          stalled.push_back(client);
        }
      }
      for (const int client : stalled) { close(client); }
    });
  }

  ~Server() {
    Worker.request_stop();
    if (Worker.joinable()) { Worker.join(); }
    if (Listener >= 0) { close(Listener); }
  }

  bool AwaitConnections(unsigned count) const {
    const auto deadline = Clock::now() + 3s;
    while (Accepted < count && Clock::now() < deadline) { std::this_thread::sleep_for(1ms); }
    return Accepted >= count;
  }
};

std::optional<outshine::Data::FetchFailureReason> AwaitFailure(outshine::Fetching &transport,
                                                               outshine::Data::Ticket ticket) {
  const auto deadline = Clock::now() + 3s;
  while (Clock::now() < deadline) {
    auto reply = transport.Collect(ticket);
    if (reply.Where() == outshine::Data::Wire::State::Unreachable) { return reply.FailureReason(); }
    if (reply.Where() != outshine::Data::Wire::State::Working) { return std::nullopt; }
    (void)transport.Await(10.0);
  }
  return std::nullopt;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::signal(SIGPIPE, SIG_IGN);
  Server server;
  CHECK(server.Port != 0, "local HTTP fixture listens");
  if (server.Port == 0) { return Report(); }
  const auto url = "http://127.0.0.1:" + std::to_string(server.Port) + "/tile";
  {
    Fetching transport({.ConcurrentTransfers = 1, .MaxRequests = 2, .TimeoutS = 10});
    CHECK(transport.WorkerCount() == 1, "one multi worker owns every transfer");
    const auto slow = transport.Begin(url);
    CHECK(slow.has_value(), "slow native transfer is admitted");
    if (!slow) { return Report(); }
    CHECK(server.AwaitConnections(1), "first request occupies the sole worker");
    const auto next = transport.Begin(url);
    CHECK(next.has_value(), "queued native transfer is admitted");
    if (!next) { return Report(); }
    const auto refused = transport.Begin(url);
    CHECK(!refused && refused.error() == Data::FetchFailureReason::CapacityRefused,
          "the bounded request table refuses work beyond its declared capacity");
    transport.Cancel(*slow);
    bool received = false;
    const auto deadline = Clock::now() + 3s;
    while (Clock::now() < deadline) {
      auto response = transport.Collect(*next).Take();
      if (response) {
        received = response->Status == 200 && response->Body == std::vector<uint8_t>({'O', 'K'});
        break;
      }
      (void)transport.Await(10.0);
    }
    CHECK(received, "cancel releases worker before the ten-second HTTP timeout");
    auto cancelled = transport.Collect(*slow);
    CHECK(cancelled.Where() == Data::Wire::State::Unreachable &&
              cancelled.FailureReason() == Data::FetchFailureReason::Cancelled,
          "cancelled transfer releases its payload but retains its actual cause");
    CHECK(transport.Collect(*slow).FailureReason() == Data::FetchFailureReason::Unavailable,
          "cancel cause is consumed once");
  }
  const auto started = Clock::now();
  {
    Fetching transport({.ConcurrentTransfers = 1, .TimeoutS = 10});
    (void)transport.Begin(url);
    CHECK(server.AwaitConnections(3), "shutdown fixture has a running transfer");
  }
  CHECK(Clock::now() - started < 3s,
        "shutdown aborts active transfer without waiting for HTTP timeout");
  {
    Fetching transport({.ConcurrentTransfers = 1, .TimeoutS = 1});
    const auto timed = transport.Begin(url);
    CHECK(server.AwaitConnections(4), "real stalled HTTP transfer starts");
    CHECK(timed.has_value(), "timeout transfer is admitted");
    if (!timed) { return Report(); }
    CHECK(AwaitFailure(transport, *timed) == Data::FetchFailureReason::TimedOut,
          "native libcurl timeout is distinct from unreachable transport");
  }
  {
    Fetching transport({.ConcurrentTransfers = 1, .MaxRequests = 2, .TimeoutS = 10});
    (void)transport.Begin(url);
    CHECK(server.AwaitConnections(5), "sole active slot prevents queued requests from starting");
    std::vector<Data::Ticket> cancelled;
    for (int i = 0; i < 5; ++i) {
      const auto queued = transport.Begin(url);
      CHECK(queued && *queued != Data::Ticket::None,
            "cancelled queue entry releases request capacity");
      if (!queued) { continue; }
      cancelled.push_back(*queued);
      transport.Cancel(*queued);
    }
    CHECK(transport.Collect(cancelled.front()).FailureReason() ==
              Data::FetchFailureReason::Unavailable,
          "bounded cancellation history forgets the oldest ticket");
    CHECK(transport.Collect(cancelled.back()).FailureReason() ==
              Data::FetchFailureReason::Cancelled,
          "newest queued cancellation retains its cause without a transfer");
  }
  {
    Server response(Server::ResponseMode::Always);
    CHECK(response.Port != 0, "body-budget fixture listens");
    Fetching transport({.TimeoutS = 2, .MaxBodyBytes = 1});
    const auto ticket =
        transport.Begin("http://127.0.0.1:" + std::to_string(response.Port) + "/tile");
    CHECK(ticket.has_value(), "body limit transfer is admitted");
    if (!ticket) { return Report(); }
    CHECK(AwaitFailure(transport, *ticket) == Data::FetchFailureReason::CapacityRefused,
          "native body budget refusal is retained through curl write failure");
  }
  {
    Fetching transport({});
    const auto empty = transport.Begin("");
    const auto embedded = transport.Begin(std::string("http://127.0.0.1/") + '\0' + "hidden");
    CHECK(!empty && empty.error() == Data::FetchFailureReason::InvalidRequest && !embedded &&
              embedded.error() == Data::FetchFailureReason::InvalidRequest,
          "empty and embedded-NUL URLs refuse before transport work");
  }
  return Report();
}
