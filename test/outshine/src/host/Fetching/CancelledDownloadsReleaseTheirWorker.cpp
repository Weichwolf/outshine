#include "Fetching.h"
#include "Check.h"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

class Server {
public:
  int Listener = socket(AF_INET, SOCK_STREAM, 0);
  uint16_t Port = 0;
  std::atomic<unsigned> Accepted{0};
  std::jthread Worker;

  Server() {
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
        if (count == 2) {
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
    Fetching transport({.Threads = 1, .TimeoutS = 10});
    const auto slow = transport.Begin(url);
    CHECK(server.AwaitConnections(1), "first request occupies the sole worker");
    const auto next = transport.Begin(url);
    transport.Cancel(slow);
    bool received = false;
    const auto deadline = Clock::now() + 3s;
    while (Clock::now() < deadline) {
      auto response = transport.Collect(next).Take();
      if (response) {
        received = response->Status == 200 && response->Body == std::vector<uint8_t>({'O', 'K'});
        break;
      }
      (void)transport.Await(10.0);
    }
    CHECK(received, "cancel releases worker before the ten-second HTTP timeout");
    CHECK(transport.Collect(slow).Where() == Data::Wire::State::Unreachable,
          "cancelled transfer has no deliverable result");
  }
  const auto started = Clock::now();
  {
    Fetching transport({.Threads = 1, .TimeoutS = 10});
    (void)transport.Begin(url);
    CHECK(server.AwaitConnections(3), "shutdown fixture has a running transfer");
  }
  CHECK(Clock::now() - started < 3s,
        "shutdown aborts active transfer without waiting for HTTP timeout");
  return Report();
}
