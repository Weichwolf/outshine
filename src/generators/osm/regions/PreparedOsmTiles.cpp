#include "PreparedOsmTileWorker.h"
#include "MvtSchema.h"
#include "AssetSourceRecipe.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>
#include <utility>

namespace outshine::Generators::Osm {
PreparedOsmTiles::Work::Work(std::string request,
                             Layout definition,
                             TileAt at,
                             const std::shared_ptr<const uint8_t> &owner,
                             AssetPipeline::Consumer consumer,
                             AssetPipeline::RequestTicket ticket)
    : Request(std::move(request)),
      Definition(std::move(definition)),
      At(at),
      Owner(owner),
      Consumer(std::move(consumer)),
      Ticket(std::move(ticket)) {}

PreparedOsmTiles::Worker::Worker(std::unique_ptr<AssetCache> cache,
                                 std::shared_ptr<Data::SourceSession> sources,
                                 Data::SourceTransport &transport,
                                 Tasks &compute,
                                 std::string recipe,
                                 Budget budget)
    : Cache_(std::move(cache)),
      Sources_(std::move(sources)),
      Transport_(transport),
      Compute_(compute),
      Recipe_(std::move(recipe)),
      Budget_(budget) {}

PreparedOsmTiles::DecodeJob::DecodeJob(std::shared_ptr<Worker> context,
                                       std::shared_ptr<Work> input,
                                       AssetPublication publication)
    : Context(std::move(context)), Input(std::move(input)), Publication(std::move(publication)) {}

PreparedOsmTiles::DecodeJob::~DecodeJob() {
  Input->Decoding.store(false, std::memory_order_release);
}

void PreparedOsmTiles::DecodeJob::Run() const {
  if (!Input->Cancelled.load()) { Context->Decode(*Input, Publication); }
}

PreparedOsmTiles::PreparedOsmTiles(std::unique_ptr<AssetCache> cache,
                                   std::shared_ptr<Data::SourceSession> sources,
                                   Data::SourceTransport &transport,
                                   Tasks &compute,
                                   std::string recipe,
                                   Budget budget)
    : Worker_(std::make_shared<Worker>(
          std::move(cache), std::move(sources), transport, compute, std::move(recipe), budget)),
      Pipeline_(compute,
                {.JobsMost = budget.ProductsMost,
                 .ConsumersMost = budget.ProductsMost,
                 .TicketsMost = budget.ProductsMost,
                 .InputBytesMost = budget.InputBytesMost,
                 .ReceiptBytesMost = budget.ProductsMost * kReceiptBytesMost}) {
  Pending_.reserve(budget.PendingMost);
}

std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
PreparedOsmTiles::Open(const std::string &directory,
                       std::shared_ptr<Data::SourceSession> sources,
                       Data::SourceTransport &transport,
                       Tasks &compute) {
  return Open(directory, std::move(sources), transport, compute, {});
}

std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
PreparedOsmTiles::Open(const std::string &directory,
                       std::shared_ptr<Data::SourceSession> sources,
                       Data::SourceTransport &transport,
                       Tasks &compute,
                       Budget budget) {
  if (!sources) { return std::unexpected("native OSM preparation requires configured sources"); }
  if (budget.PendingMost == 0 || budget.PublicationBytesMost == 0 || budget.ProductsMost == 0 ||
      budget.InputBytesMost == 0 ||
      budget.ProductsMost > std::numeric_limits<size_t>::max() / kReceiptBytesMost) {
    return std::unexpected("invalid native OSM admission budget");
  }
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) { return std::unexpected("could not create native OSM tile cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open native OSM tile cache"); }
  const auto recipe = Generators::AssetSourceRecipe(
      "prepared-osm-tile-3", sources->Sources(), std::array{Data::DataKind::VectorMap});
  if (recipe.empty()) { return std::unexpected("OSM source metadata cannot be encoded"); }
  return std::shared_ptr<PreparedOsmTiles>(new PreparedOsmTiles(
      std::move(*cache), std::move(sources), transport, compute, recipe, budget));
}

PreparedOsmTiles::~PreparedOsmTiles() {
  for (const auto &work : Pending_) {
    work->Cancelled.store(true);
    if (work->Ticket) { work->Ticket->Cancel(); }
  }
  auto &compute = Worker_->Compute_;
  (void)compute.PostDetached([worker = std::move(Worker_), pending = std::move(Pending_)] {});
}

bool PreparedOsmTiles::DecodeIdle(const Work &work) {
  return !work.Decoding.load(std::memory_order_acquire);
}

void PreparedOsmTiles::RetireUnused() {
  std::erase_if(Pending_, [](const auto &work) {
    if (!work->Owner.expired()) { return false; }
    work->Cancelled.store(true);
    if (work->Ticket) { work->Ticket->Cancel(); }
    return DecodeIdle(*work) && (!work->Ticket || work->Ticket->Poll().Finished);
  });
}

void PreparedOsmTiles::RetainPending(const OsmField &layout,
                                     const std::optional<OsmTileWindow> &window) {
  for (const auto &work : Pending_) {
    if (work->Owner.lock() != layout.Demand_) { continue; }
    const auto &definition = work->Definition;
    if (!window || definition.Zoom != layout.Zoom_ || definition.Schema != layout.Schema_ ||
        definition.Layers != layout.Layers_ || work->At.X < window->MinX ||
        work->At.X > window->MaxX || work->At.Y < window->MinY || work->At.Y > window->MaxY) {
      work->Owner.reset();
    }
  }
  RetireUnused();
}

std::string PreparedOsmTiles::Worker::RequestKey(TileAt at, const Layout &layout) const {
  BinaryValueWriter out(4096);
  if (!out.Text(Recipe_) || !out(layout.Zoom, at.X, at.Y, layout.Schema)) { return {}; }
  for (const auto &layer : layout.Layers) {
    if (!out.Text(layer)) { return {}; }
    if (layout.Schema == MvtSchema::OpenMapTiles && (layer == "land" || layer == "sites") &&
        !out.Text(MvtLayerName(layout.Schema, layer))) {
      return {};
    }
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::expected<PreparedOsmTiles::SourceDelivery, std::string_view>
PreparedOsmTiles::Acquire(TileAt at, const OsmField &layout) {
  if (layout.Zoom_ < 0 || layout.Zoom_ >= std::numeric_limits<int>::digits || at.X < 0 ||
      at.Y < 0 ||
      std::cmp_greater_equal(at.X, uint32_t{1} << static_cast<unsigned>(layout.Zoom_)) ||
      std::cmp_greater_equal(at.Y, uint32_t{1} << static_cast<unsigned>(layout.Zoom_))) {
    return std::unexpected("invalid native OSM tile address");
  }
  RetireUnused();
  const auto found = std::ranges::find_if(Pending_, [&](const auto &work) {
    return work->At.X == at.X && work->At.Y == at.Y && work->Owner.lock() == layout.Demand_ &&
           work->Definition.Zoom == layout.Zoom_ && work->Definition.Schema == layout.Schema_ &&
           work->Definition.Layers == layout.Layers_;
  });
  if (found != Pending_.end()) {
    auto result = Advance(*found);
    if (result &&
        (result->Status == State::Ready || result->Status == State::Absent || (*found)->Retry)) {
      Pending_.erase(found);
    }
    return result;
  }
  const Layout definition{.Zoom = layout.Zoom_, .Schema = layout.Schema_, .Layers = layout.Layers_};
  const auto request = Worker_->RequestKey(at, definition);
  if (request.empty()) { return std::unexpected("invalid native OSM tile request"); }
  if (Pending_.size() >= Worker_->Budget_.PendingMost) { return SourceDelivery{}; }
  auto consumer = Pipeline_.OpenConsumer();
  if (!consumer) { return SourceDelivery{}; }
  const auto production = std::make_shared<Production>(Worker_, definition, at, request);
  auto ticket = Pipeline_.RequestLogical(*consumer,
                                         {.Address = request,
                                          .Generation = consumer->Generation(),
                                          .ReceiptBytesMost = kReceiptBytesMost},
                                         [production](const std::stop_token &stop) {
                                           return production->Owner->Resolve(production, stop);
                                         });
  if (!ticket) {
    if (ticket.error() == AssetPublicationError::ResourceDeferred) { return SourceDelivery{}; }
    return std::unexpected("native OSM asset job was not accepted");
  }
  auto work = std::make_shared<Work>(
      request, definition, at, layout.Demand_, std::move(*consumer), std::move(*ticket));
  Pending_.push_back(std::move(work));
  return SourceDelivery{};
}

std::expected<PreparedOsmTiles::SourceDelivery, std::string_view>
PreparedOsmTiles::Advance(const std::shared_ptr<Work> &work) {
  if (!DecodeIdle(*work)) { return SourceDelivery{}; }
  if (!work->Result) { return std::unexpected(std::string_view(work->Result.error())); }
  if (*work->Result) {
    work->Ticket.reset();
    const Product product(work, work->Result->get());
    return SourceDelivery{.Status = State::Ready, .Product = product};
  }
  if (work->Retry) { return SourceDelivery{}; }
  if (!work->Ticket) { return std::unexpected("native OSM demand has no ticket"); }
  auto publication = work->Ticket->Poll();
  switch (publication.Status) {
    case AssetPublicationStatus::Pending: return SourceDelivery{};
    case AssetPublicationStatus::Cancelled:
      return std::unexpected("native OSM preparation cancelled");
    case AssetPublicationStatus::Failed:
      if (publication.Error == AssetPublicationError::Rejected) {
        work->Retry = true;
        if (work->Ticket) { work->Ticket->Cancel(); }
        return SourceDelivery{};
      }
      if (publication.Message == kAbsent) {
        return SourceDelivery{.Status = State::Absent, .Product = {}};
      }
      if (publication.Message == kRefused) {
        return SourceDelivery{.Status = State::Refused, .Product = {}};
      }
      return std::unexpected("native OSM asset job failed");
    case AssetPublicationStatus::Ready: break;
  }
  work->Decoding.store(true, std::memory_order_release);
  const auto job = std::make_shared<DecodeJob>(Worker_, work, std::move(publication));
  if (!Worker_->Compute_.PostDetached([job] { job->Run(); })) {
    return std::unexpected("native OSM decode task was not accepted");
  }
  return SourceDelivery{};
}

AssetPipeline::Snapshot PreparedOsmTiles::AdmissionCounts() const noexcept {
  return Pipeline_.Counts();
}

PreparedOsmTiles::Counters PreparedOsmTiles::Costs() const noexcept {
  return {.Hits = Worker_->Hits_.load(),
          .Misses = Worker_->Misses_.load(),
          .Writes = Worker_->Writes_.load(),
          .ReadBytes = Worker_->ReadBytes_.load(),
          .ReadMs = Worker_->ReadMs_.load(),
          .GenerationMs = Worker_->GenerationMs_.load(),
          .Generations = Worker_->Generations_.load(),
          .PublicationAttempts = Worker_->PublicationAttempts_.load(),
          .PublicationDeferred = Worker_->PublicationDeferred_.load(),
          .PublicationBytes = Worker_->PublicationBytes_.load(),
          .PublicationPeakBytes = Worker_->PublicationPeakBytes_.load(),
          .PublicationReadBytes = Worker_->PublicationReadBytes_.load(),
          .PublicationMs = Worker_->PublicationMs_.load()};
}
}
