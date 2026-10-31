#include "Check.h"
#include "HttpTransport.h"
#include "LoopbackHttpServer.h"
#include "SourceTransport.h"
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <print>
#include <string>
#include <utility>
#include <zlib.h>

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

std::string Encode(std::string input, int windowBits) {
  z_stream stream{};
  const int initialized =
      deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY);
  CHECK(initialized == Z_OK, "the declared finite-window response codec initializes");
  if (initialized != Z_OK) { return {}; }
  std::string output(deflateBound(&stream, static_cast<uLong>(input.size())), '\0');
  stream.next_in = reinterpret_cast<Bytef *>(input.data());
  stream.avail_in = static_cast<uInt>(input.size());
  stream.next_out = reinterpret_cast<Bytef *>(output.data());
  stream.avail_out = static_cast<uInt>(output.size());
  const int encoded = deflate(&stream, Z_FINISH);
  const auto written = stream.total_out;
  const int ended = deflateEnd(&stream);
  CHECK(encoded == Z_STREAM_END && ended == Z_OK, "the complete controlled response is encoded");
  output.resize(written);
  return output;
}

void Coding(std::string coding, std::string body, bool accepted, size_t most = 30) {
  using namespace outshine::Test;
  const auto label = coding;
  const LoopbackHttpServer server(
      [coding = std::move(coding), body = std::move(body)](const std::string & /*request*/) {
        return "HTTP/1.1 200 OK\r\nContent-Encoding: " + coding +
               "\r\nContent-Length: " + std::to_string(body.size()) +
               "\r\nConnection: close\r\n\r\n" + body;
      });
  CHECK(server.Ready(), "a real endpoint serves the declared response coding");
  if (!server.Ready()) { return; }
  HttpTransport wire({.ConcurrentTransfers = 1});
  const auto start = wire.Begin(server.Url(), most);
  CHECK(start, "the bounded encoded request reaches the native HTTP worker");
  if (!start) { return; }
  auto response = Await(wire, *start);
  if (accepted) {
    const auto delivered = response.Take();
    CHECK(delivered && delivered->Status == 200 && delivered->Body.capacity() <= most &&
              std::string(delivered->Body.begin(), delivered->Body.end()) == std::string(30, 'a'),
          "a supported complete response keeps exactly the decoded bytes within admission");
  } else {
    if (response.Where() != Wire::State::Unreachable ||
        response.FailureReason() != SourceFailureReason::CapacityRefused) {
      std::println(stderr, "response coding={} failure={}", label, Name(response.FailureReason()));
    }
    CHECK(response.Where() == Wire::State::Unreachable &&
              response.FailureReason() == SourceFailureReason::CapacityRefused,
          "an unplanned response codec or excess decoded byte is refused before retention");
  }
  const auto requests = server.Requests();
  CHECK(requests.size() == 1, "one wire request exercises the response coding contract");
  if (requests.size() != 1) { return; }
  const auto began = requests.front().Text.find("Accept-Encoding:");
  const auto end = requests.front().Text.find("\r\n", began);
  CHECK(began != std::string::npos && end != std::string::npos,
        "the request declares its response codec policy");
  if (began == std::string::npos || end == std::string::npos) { return; }
  const auto advertised = requests.front().Text.substr(began, end - began);
  CHECK(advertised.contains("gzip") && advertised.contains("deflate") &&
            !advertised.contains("br") && !advertised.contains("zstd"),
        "only codecs with the admitted fixed-window contract are advertised");
}

void RedirectedCoding() {
  using namespace outshine::Test;
  const auto empty = Encode({}, MAX_WBITS + 16);
  const auto body = Encode(std::string(30, 'a'), MAX_WBITS + 16);
  const LoopbackHttpServer server([&](const std::string &request) {
    if (request.starts_with("GET /final ")) {
      return "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: " +
             std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    }
    return "HTTP/1.1 302 Found\r\nLocation: /final\r\nContent-Encoding: gzip\r\nContent-Length: " +
           std::to_string(empty.size()) + "\r\nConnection: close\r\n\r\n" + empty;
  });
  CHECK(server.Ready(), "a compressed redirect endpoint opens");
  if (!server.Ready()) { return; }
  HttpTransport wire({.ConcurrentTransfers = 1});
  const auto start = wire.Begin(server.Url(), 30);
  CHECK(start, "the encoded redirect uses the same finite request admission");
  if (!start) { return; }
  const auto response = Await(wire, *start).Take();
  CHECK(response && response->Status == 200 && response->Body.capacity() <= 30 &&
            std::string(response->Body.begin(), response->Body.end()) == std::string(30, 'a') &&
            server.Requests().size() == 2,
        "a new response resets its codec history while retaining the decoded body bound");
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
  const std::string plain(30, 'a');
  const auto gzip = Encode(plain, MAX_WBITS + 16);
  Coding("identity", plain, true);
  Coding("gzip", gzip, true);
  Coding("GZip", gzip, true);
  Coding("deflate", Encode(plain, MAX_WBITS), true);
  Coding("deflate", Encode(plain, -MAX_WBITS), true);
  Coding("gzip", gzip, false, 29);
  const std::string wideWindow("\x28\xb5\x2f\xfd\x00\x88\x09\x00\x00\x61", 10);
  Coding("zstd", wideWindow, false);
  Coding("br", {}, false);
  Coding("gzip, gzip", Encode(gzip, MAX_WBITS + 16), false);
  Coding("gzip\r\nContent-Encoding: gzip", Encode(gzip, MAX_WBITS + 16), false);
  Coding("GZip\r\ncOnTenT-EnCoDiNg: gzip", Encode(gzip, MAX_WBITS + 16), false);
  RedirectedCoding();
  return Report();
}
