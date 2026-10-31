#include "SourceSession.h"
#include "Check.h"
#include <array>
#include <cstdlib>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;
using namespace outshine::Test;

struct Calls {
  unsigned Made = 0;
  unsigned Destroyed = 0;
};

constexpr auto kInvalidSourceKind = static_cast<DataKind>(255);

class SnapshotSource final : public Source {
public:
  SnapshotSource(std::shared_ptr<Calls> calls, SourceDecl declaration)
      : Calls_(std::move(calls)), Decl_(std::move(declaration)) {}

  ~SnapshotSource() override { ++Calls_->Destroyed; }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage
  Covers([[maybe_unused]] const SourceRequest &request) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin([[maybe_unused]] const Address &where,
                                 [[maybe_unused]] SourceTransport &transport) const override {
    return Ticket{1};
  }

  [[nodiscard]] SourceResponse Collect([[maybe_unused]] const Address &where,
                                       [[maybe_unused]] Ticket ticket,
                                       [[maybe_unused]] SourceTransport &transport) const override {
    return SourceResponse::Delivered({2, 3, 5});
  }

private:
  std::shared_ptr<Calls> Calls_;
  SourceDecl Decl_;
};

class SnapshotProvider final : public Provider {
public:
  explicit SnapshotProvider(std::shared_ptr<Calls> calls) : Calls_(std::move(calls)) {}

  [[nodiscard]] std::string_view kind() const override { return "prepared-source"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &declaration, [[maybe_unused]] std::string_view root) const override {
    ++Calls_->Made;
    SourceDecl source;
    source.Id = declaration.Dataset == "unnamed" ? "" : "prepared-source";
    source.Revision = declaration.Revision;
    source.MaxZoom = 10;
    source.Keeps = Cacheability::Never;
    if (declaration.Dataset == "invalid") { source.Kind = kInvalidSourceKind; }
    return std::make_unique<SnapshotSource>(Calls_, std::move(source));
  }

private:
  std::shared_ptr<Calls> Calls_;
};

class NoWire final : public SourceTransport {
public:
  [[nodiscard]] FetchStart Begin([[maybe_unused]] const std::string &url) override {
    return Ticket::None;
  }

  [[nodiscard]] Wire Collect([[maybe_unused]] Ticket ticket) override { return Wire::Working(); }

  void Cancel([[maybe_unused]] Ticket ticket) override {}
};

std::expected<SourceSession::Configuration, std::string>
Prepare(const std::shared_ptr<Calls> &calls) {
  const SnapshotProvider factory(calls);
  ProviderRegistry registry;
  if (!registry.registerProvider(factory)) {
    return std::unexpected("fixture registration failed");
  }
  SourceProvider declaration;
  declaration.Kind = "prepared-source";
  declaration.Revision = "frozen";
  const std::array declarations{declaration};
  return SourceSession::Prepare(declarations, {}, registry);
}

void Transfer(const std::filesystem::path &cache) {
  const auto calls = std::make_shared<Calls>();
  auto prepared = Prepare(calls);
  CHECK(prepared && calls->Made == 1 && calls->Destroyed == 0 && !std::filesystem::exists(cache),
        "preparation owns one source after factory and registry destruction without cache IO");
  if (!prepared) { return; }
  auto transferred = std::move(*prepared);
  const auto rejectedPath = cache.parent_path() / "already-consumed";
  const auto rejected =
      SourceSession::Open({.Directory = rejectedPath.string()}, std::move(*prepared));
  CHECK(!rejected && rejected.error().contains("consumed") &&
            !std::filesystem::exists(rejectedPath),
        "a moved-from configuration cannot open a cache or duplicate the source");
  std::expected<std::shared_ptr<SourceSession>, std::string> session =
      std::unexpected("not opened");
  {
    const std::jthread worker([&session, cache, configuration = std::move(transferred)] mutable {
      session = SourceSession::Open({.Directory = cache.string()}, std::move(configuration));
    });
  }
  CHECK(session && calls->Made == 1 && calls->Destroyed == 0 && !std::filesystem::exists(cache),
        "session binding consumes the original source without factory calls or cache IO");
  if (!session) { return; }
  CHECK((*session)->Sources().Count() == 1 &&
            (*session)->Sources().At(0).Declaration().Revision == "frozen",
        "the opened session retains the prepared immutable identity");
  NoWire wire;
  auto read = SourceSession::Request(
      *session, SourceRequest(DataKind::Elevation, Address::At({.Zoom = 3, .X = 1, .Y = 2})), wire);
  CHECK(read.has_value(), "the transferred source serves actual native requests");
  if (!read) { return; }
  auto answer = read->Poll();
  for (int attempt = 0; attempt < 3 && answer.Where() == SourceDelivery::State::Pending;
       ++attempt) {
    answer = read->Poll();
  }
  CHECK(answer.Where() == SourceDelivery::State::Delivered,
        "the originally prepared source delivers after all configuration inputs are gone");
  const auto payload = answer.Take();
  CHECK(payload && payload->SourceRevision == "frozen" &&
            payload->Bytes == std::vector<uint8_t>({2, 3, 5}),
        "native delivery preserves the prepared identity and bytes");
  CHECK(!std::filesystem::exists(cache), "uncacheable sources never open the raw cache");
  read->Cancel();
  session->reset();
  CHECK(calls->Destroyed == 1, "source ownership ends exactly once after the final consumer");
}

void RejectedMetadata(const std::filesystem::path &cache) {
  const auto calls = std::make_shared<Calls>();
  const SnapshotProvider factory(calls);
  ProviderRegistry registry;
  CHECK(registry.registerProvider(factory), "metadata fixture provider registered");
  for (const auto *dataset : {"unnamed", "invalid"}) {
    SourceProvider declaration;
    declaration.Kind = "prepared-source";
    declaration.Dataset = dataset;
    const auto refused = SourceSession::Open(
        {.Directory = cache.string()}, std::span(&declaration, 1), {}, registry);
    CHECK(!refused && !std::filesystem::exists(cache) && calls->Made == calls->Destroyed,
          "invalid resolved identity destroys its source before cache IO");
  }
  std::array<SourceProvider, 2> declarations;
  declarations[0].Kind = declarations[1].Kind = "prepared-source";
  declarations[1].Priority = 1;
  const auto duplicate =
      SourceSession::Open({.Directory = cache.string()}, declarations, {}, registry);
  CHECK(!duplicate && duplicate.error().contains("priority twice") &&
            !std::filesystem::exists(cache) && calls->Made == calls->Destroyed,
        "different declaration ranks cannot conceal conflicting resolved source ranks");
}
}

int main() {
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-prepared-sources-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache parent created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  RejectedMetadata(std::filesystem::path(directory) / "invalid");
  Transfer(std::filesystem::path(directory) / "cache");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated caches removed");
  return Report();
}
