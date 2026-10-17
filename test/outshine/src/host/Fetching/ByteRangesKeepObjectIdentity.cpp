#include <system_error>
#include <span>
#include <memory>
#include <filesystem>
#include <array>
#include "SourceSet.h"
#include "OfflineTransport.h"
#include "ContentStore.h"
#include "CopernicusDem.h"
#include "CopernicusRaster.h"
#include "Fetching.h"
#include "Check.h"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <optional>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using outshine::Data::ByteRange;
using outshine::Data::FetchFailureReason;
using outshine::Data::Wire;

class Server {
public:
  int Listener = socket(AF_INET, SOCK_STREAM, 0);
  uint16_t Port = 0;
  std::mutex Mutex;
  std::vector<std::string> Requests;
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
      while (!stop.stop_requested()) {
        pollfd pending{.fd = Listener, .events = POLLIN, .revents = 0};
        if (poll(&pending, 1, 20) <= 0) { continue; }
        const int client = accept(Listener, nullptr, nullptr);
        if (client < 0) { continue; }
        std::string request;
        const auto deadline = Clock::now() + 1s;
        while (request.size() < 8192 && !request.contains("\r\n\r\n") && Clock::now() < deadline) {
          pollfd readable{.fd = client, .events = POLLIN, .revents = 0};
          if (poll(&readable, 1, 20) <= 0) { continue; }
          char bytes[1024];
          const auto count = recv(client, bytes, sizeof(bytes), 0);
          if (count <= 0) { break; }
          request.append(bytes, static_cast<size_t>(count));
        }
        {
          const std::scoped_lock lock(Mutex);
          Requests.push_back(request);
        }
        const auto first = request.find(' ');
        const auto last = request.find(' ', first + 1);
        const auto path = request.substr(first + 1, last - first - 1);
        std::string headers = "Content-Range: bytes 4-7/64\r\nETag: \"v1\"\r\n";
        std::string body = "DATA";
        int status = 206;
        if (path == "/full") {
          status = 200;
          headers.clear();
        }
        if (path == "/offset") { headers = "Content-Range: bytes 3-6/64\r\nETag: \"v1\"\r\n"; }
        if (path == "/length") { body = "DAT"; }
        if (path == "/large") { body = "EXTRA"; }
        if (path == "/weak") { headers = "Content-Range: bytes 4-7/64\r\nETag: W/\"v1\"\r\n"; }
        if (path == "/revision") { headers = "Content-Range: bytes 4-7/64\r\nETag: \"v2\"\r\n"; }
        if (path == "/noetag") { headers = "Content-Range: bytes 4-7/64\r\n"; }
        if (path == "/norange") { headers = "ETag: \"v1\"\r\n"; }
        if (path == "/unknown") { headers = "Content-Range: bytes 4-7/*\r\nETag: \"v1\"\r\n"; }
        if (path == "/total") { headers = "Content-Range: bytes 4-7/7\r\nETag: \"v1\"\r\n"; }
        if (path == "/duplicate") { headers += "Content-Range: bytes 4-7/64\r\n"; }
        if (path == "/encoding") { headers += "Content-Encoding: gzip\r\n"; }
        if (path == "/precondition") {
          status = 412;
          headers.clear();
          body.clear();
        }
        if (path == "/redirect") {
          status = 302;
          headers = "Location: /valid\r\nContent-Range: bytes 9-12/64\r\nETag: \"old\"\r\n";
          body.clear();
        }
        std::string response = "HTTP/1.1 " + std::to_string(status) + " Fixture\r\n" + headers +
                               "Content-Length: " + std::to_string(body.size()) +
                               "\r\nConnection: close\r\n\r\n" + body;
        if (path == "/continue") {
          response.insert(0, "HTTP/1.1 100 Continue\r\nETag: \"old\"\r\n\r\n");
        }
        (void)send(client, response.data(), response.size(), 0);
        close(client);
      }
    });
  }

  ~Server() {
    Worker.request_stop();
    if (Worker.joinable()) { Worker.join(); }
    if (Listener >= 0) { close(Listener); }
  }
};

Wire Await(outshine::Fetching &transport,
           outshine::Data::Ticket ticket,
           std::chrono::seconds budget = 3s) {
  const auto deadline = Clock::now() + budget;
  while (Clock::now() < deadline) {
    auto reply = transport.Collect(ticket);
    if (reply.Where() != Wire::State::Working) { return reply; }
    (void)transport.Await(10.0);
  }
  transport.Cancel(ticket);
  return Wire::Unreachable(FetchFailureReason::TimedOut);
}

using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Test;

std::expected<Delivery::Answer, FetchFailureReason>
Acquire(SourceSet &sources, Transport &wire, const Fetch &request) {
  auto query = sources.Ask(request);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
  while (std::chrono::steady_clock::now() < deadline) {
    auto response = sources.Collect(query, wire);
    if (response.Where() != Delivery::State::Pending) {
      auto settled = response.Take();
      if (!settled) {
        return std::unexpected(response.Failure() ? response.Failure()->Reason
                                                  : FetchFailureReason::Unavailable);
      }
      return std::move(*settled);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  SourceSet::Abandon(query, wire);
  return std::unexpected(FetchFailureReason::Unavailable);
}

void RealOriginal(const char *url) {
  Fetching wire({});
  auto directory = (std::filesystem::temp_directory_path() / "outshine-cog-live-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-network cache");
  const ContentStore::Config config{.Directory = directory};
  ContentStore store(config);
  SourceSet sources(store);
  const auto at = Address::AtCell({54, 9});
  auto provider = std::make_unique<CopernicusDem>();
  CHECK(provider->Url(at) == url, "probe uses the official native N54/E009 source address");
  CHECK(sources.Add(std::move(provider)) == SourceSet::Registration::Accepted,
        "live native source uses the shared registry and raw cache");
  const Fetch headerRequest(DataKind::Elevation, at, ByteRange{.First = 0, .Length = 16384});
  auto header = Acquire(sources, wire, headerRequest);
  CHECK(header && header->Range, "real COG metadata has a verified range receipt");
  if (!header || !header->Range) { return; }
  std::array<CopernicusPart, 2> parts;
  parts[0] = {.ObjectKey = url, .Origin = *header->Range, .Bytes = header->Bytes};
  const CopernicusObject metadataObject{.ObjectKey = url,
                                        .TotalBytes = header->Range->TotalBytes,
                                        .EntityTag = header->Range->EntityTag,
                                        .Parts = std::span(parts).first(1)};
  const auto metadata = ReadCopernicusRaster(metadataObject);
  CHECK(metadata && metadata->Levels.size() == 4, "real N54/E009 has four native resolutions");
  if (!metadata) { return; }
  const auto &native = metadata->Levels.front();
  CHECK(native.Columns == 2400 && native.Rows == 3600 && native.BlockColumns == 1024 &&
            native.BlockRows == 1024 && native.WestDeg == 9 && native.NorthDeg == 55,
        "real source georeference survives native decoding");
  const Fetch blockRequest(DataKind::Elevation, at, native.Blocks[1], header->Range->EntityTag);
  auto bytes = Acquire(sources, wire, blockRequest);
  CHECK(bytes && bytes->Range,
        "one spatially selected block keeps the same strong object revision");
  if (!bytes || !bytes->Range) { return; }
  parts[1] = {.ObjectKey = url, .Origin = *bytes->Range, .Bytes = bytes->Bytes};
  auto source = metadataObject;
  source.Parts = parts;
  const auto begin = std::chrono::steady_clock::now();
  const auto block = ReadCopernicusBlock(source, *metadata, 0, 1);
  const double decodeMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
  CHECK(block && block->FirstColumn == 1024 && block->FirstRow == 0,
        "selected original block decodes without loading the complete file");
  if (!block) { return; }
  const auto height = block->HeightM(781, 20);
  CHECK(height, "Flensburg diagnostic sample is present");
  std::printf(
      "ORIGINAL received=%zu total=%llu revision=%s sample(1044,781)=%g m EGM2008 decode=%.3f ms\n",
      header->Bytes.size() + bytes->Bytes.size(),
      static_cast<unsigned long long>(source.TotalBytes),
      header->Range->EntityTag.c_str(),
      height.value_or(std::numeric_limits<float>::quiet_NaN()),
      decodeMs);
  ContentStore reopened(config);
  SourceSet warm(reopened);
  CHECK(warm.Add(std::make_unique<CopernicusDem>()) == SourceSet::Registration::Accepted,
        "warm source starts with a newly opened cache and registry");
  OfflineTransport offline;
  auto cachedHeader = Acquire(warm, offline, headerRequest);
  auto cachedBlock = Acquire(warm, offline, blockRequest);
  CHECK(cachedHeader && cachedBlock && cachedHeader->Range && cachedBlock->Range &&
            cachedHeader->Bytes == header->Bytes && cachedBlock->Bytes == bytes->Bytes &&
            cachedHeader->Range->EntityTag == header->Range->EntityTag &&
            cachedBlock->Range->TotalBytes == source.TotalBytes && warm.Counters().FromStore == 2 &&
            warm.Counters().ProviderStarts == 0,
        "reopened offline cache reproduces exact original COG ranges and their receipts");
  if (cachedHeader && cachedBlock && cachedHeader->Range && cachedBlock->Range) {
    parts[0] = {.ObjectKey = url, .Origin = *cachedHeader->Range, .Bytes = cachedHeader->Bytes};
    parts[1] = {.ObjectKey = url, .Origin = *cachedBlock->Range, .Bytes = cachedBlock->Bytes};
    const auto again = ReadCopernicusBlock(source, *metadata, 0, 1);
    CHECK(again && again->HeightM(781, 20) == height,
          "cached original bytes produce the same native EGM2008 meter sample without network IO");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "probe cache fixture removed");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::signal(SIGPIPE, SIG_IGN);
  Server server;
  CHECK(server.Port != 0, "local byte-range fixture listens");
  if (server.Port == 0) { return Report(); }
  const std::string url = "http://127.0.0.1:" + std::to_string(server.Port);
  Fetching transport({.ConcurrentTransfers = 1, .MaxRequests = 32, .MaxBodyBytes = 8});
  const ByteRange range{.First = 4, .Length = 4};
  for (const std::string_view path : {"/valid", "/redirect", "/continue"}) {
    const auto started = transport.Begin(url + std::string(path), range, "\"v1\"");
    CHECK(started.has_value(), "pinned range enters the existing transport");
    if (!started) { continue; }
    auto reply = Await(transport, *started);
    auto moved = std::move(reply);
    auto response = moved.Take();
    CHECK(response && response->Status == 206 &&
              response->Body == std::vector<uint8_t>({'D', 'A', 'T', 'A'}) && response->Range &&
              response->Range->Bytes == range && response->Range->TotalBytes == 64 &&
              response->Range->EntityTag == "\"v1\"",
          "range bytes and source identity survive movement, redirects and intermediate headers");
    CHECK(!moved.Take(), "range response ownership transfers once");
  }
  for (const std::string_view path : {"/full",
                                      "/offset",
                                      "/length",
                                      "/weak",
                                      "/revision",
                                      "/noetag",
                                      "/norange",
                                      "/unknown",
                                      "/total",
                                      "/duplicate",
                                      "/encoding",
                                      "/large"}) {
    const auto started = transport.Begin(url + std::string(path), range, "\"v1\"");
    CHECK(started.has_value(), "adversarial range request is admitted");
    if (!started) { continue; }
    auto reply = Await(transport, *started);
    CHECK(reply.Where() == Wire::State::Unreachable &&
              reply.FailureReason() == (path == "/large" ? FetchFailureReason::CapacityRefused
                                                         : FetchFailureReason::CorruptPayload),
          "invalid success cannot become cached source bytes");
  }
  for (const ByteRange invalid :
       {ByteRange{.First = 4, .Length = 0},
        ByteRange{.First = std::numeric_limits<uint64_t>::max(), .Length = 2},
        ByteRange{.First = 0, .Length = 9}}) {
    const auto started = transport.Begin(url + "/valid", invalid);
    CHECK(!started && started.error() == (invalid.Length == 9 ? FetchFailureReason::CapacityRefused
                                                              : FetchFailureReason::InvalidRequest),
          "invalid or over-budget ranges fail before occupying a queue entry");
  }
  for (const std::string_view invalid : {"W/\"v1\"", "unquoted", "\"v1\"\r\nInjected: yes"}) {
    const auto started = transport.Begin(url + "/valid", range, invalid);
    CHECK(!started && started.error() == FetchFailureReason::InvalidRequest,
          "weak and malformed revision pins never become request headers");
  }
  const auto failedPin = transport.Begin(url + "/precondition", range, "\"v1\"");
  CHECK(failedPin.has_value(), "precondition request starts");
  if (failedPin) {
    const auto response = Await(transport, *failedPin).Take();
    CHECK(response && response->Status == 412 && !response->Range,
          "HTTP precondition failure remains an HTTP error without valid range metadata");
  }
  const auto full = transport.Begin(url + "/full");
  CHECK(full.has_value(), "full-object request still starts");
  if (full) {
    const auto response = Await(transport, *full).Take();
    CHECK(response && response->Status == 200 && response->Body.size() == 4 && !response->Range,
          "ordinary acquisition retains its existing response contract");
  }
  {
    const std::scoped_lock lock(server.Mutex);
    CHECK(server.Requests.size() == 18, "invalid starts issue no hidden network work");
    CHECK(!server.Requests.empty() && server.Requests.front().contains("Range: bytes=4-7\r\n") &&
              server.Requests.front().contains("If-Match: \"v1\"\r\n") &&
              server.Requests.front().contains("Accept-Encoding: identity\r\n"),
          "actual HTTP request pins the unencoded byte interval and object revision");
  }
  if (const char *probe = std::getenv("OUTSHINE_COG_RANGE_PROBE")) {
    Fetching remote({.ConcurrentTransfers = 1, .TimeoutS = 25, .MaxBodyBytes = 16384});
    const auto started = remote.Begin(probe, ByteRange{.First = 0, .Length = 16384});
    CHECK(started.has_value(), "explicit real COG probe starts");
    if (started) {
      const auto response = Await(remote, *started, 30s).Take();
      CHECK(response && response->Range && response->Range->TotalBytes > response->Body.size() &&
                response->Body.size() == 16384 && response->Body[0] == 'I' &&
                response->Body[1] == 'I',
            "real Copernicus header arrives without downloading the complete raster");
      if (response && response->Range) {
        std::printf("COG probe: %zu/%llu bytes; ETag %s\n",
                    response->Body.size(),
                    static_cast<unsigned long long>(response->Range->TotalBytes),
                    response->Range->EntityTag.c_str());
      }
    }
  }
  if (const char *original = std::getenv("OUTSHINE_GLO30_RASTER_PROBE")) { RealOriginal(original); }
  return Report();
}
