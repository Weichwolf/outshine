#ifndef OUTSHINE_TEST_LOOPBACKHTTPSERVER_H
#define OUTSHINE_TEST_LOOPBACKHTTPSERVER_H

#include <arpa/inet.h>
#include <chrono>
#include <functional>
#include <mutex>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <unistd.h>
#include <vector>

namespace outshine::Test {
class LoopbackHttpServer {
public:
  using Clock = std::chrono::steady_clock;

  struct Request {
    std::string Text;
    Clock::time_point At;
  };

  using Respond = std::function<std::string(const std::string &)>;

  explicit LoopbackHttpServer(Respond respond) : Respond_(std::move(respond)) {
    Listener_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (Listener_ < 0 ||
        bind(Listener_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(Listener_, 8) != 0) {
      return;
    }
    socklen_t size = sizeof(address);
    if (getsockname(Listener_, reinterpret_cast<sockaddr *>(&address), &size) != 0) { return; }
    Port_ = ntohs(address.sin_port);
    Worker_ = std::jthread([this](std::stop_token stop) { Serve(stop); });
  }

  ~LoopbackHttpServer() {
    Worker_.request_stop();
    if (Worker_.joinable()) { Worker_.join(); }
    if (Listener_ >= 0) { close(Listener_); }
  }

  [[nodiscard]] std::string Url(std::string_view host = "localhost") const {
    return "http://" + std::string(host) + ":" + std::to_string(Port_);
  }

  [[nodiscard]] bool Ready() const { return Port_ != 0; }

  [[nodiscard]] std::vector<Request> Requests() const {
    const std::scoped_lock lock(Mutex_);
    return Requests_;
  }

private:
  void Receive(int client) {
    std::string request;
    const auto deadline = Clock::now() + std::chrono::seconds(1);
    while (request.size() < 8192 && !request.contains("\r\n\r\n") && Clock::now() < deadline) {
      pollfd readable{.fd = client, .events = POLLIN, .revents = 0};
      if (poll(&readable, 1, 20) <= 0) { continue; }
      char bytes[1024];
      const auto count = recv(client, bytes, sizeof(bytes), 0);
      if (count <= 0) { return; }
      request.append(bytes, static_cast<size_t>(count));
    }
    {
      const std::scoped_lock lock(Mutex_);
      Requests_.push_back({request, Clock::now()});
    }
    const auto response = Respond_(request);
    size_t sent = 0;
    while (sent < response.size()) {
      const auto count = send(client, response.data() + sent, response.size() - sent, 0);
      if (count <= 0) { break; }
      sent += static_cast<size_t>(count);
    }
  }

  void Serve(std::stop_token stop) {
    while (!stop.stop_requested()) {
      pollfd pending{.fd = Listener_, .events = POLLIN, .revents = 0};
      if (poll(&pending, 1, 20) <= 0) { continue; }
      const int client = accept(Listener_, nullptr, nullptr);
      if (client < 0) { continue; }
      Receive(client);
      close(client);
    }
  }

  int Listener_ = -1;
  uint16_t Port_ = 0;
  Respond Respond_;
  mutable std::mutex Mutex_;
  std::vector<Request> Requests_;
  std::jthread Worker_;
};
}
#endif
