#include "WeatherSnapshotLoader.h"
#include "SourceSession.h"
#include <DataKind.h>
#include <SourceRequest.h>
#include <utility>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace outshine::Generators::Weather {
struct WeatherSnapshotLoader::Work {
  Work(std::shared_ptr<Data::SourceSession> sources, int64_t utcS);
  std::shared_ptr<Data::SourceSession> Sources;
  Data::SourceSet::Query Query;
  int64_t UtcS;
  std::atomic_bool Stopping = false;
  Data::SourceDelivery Delivery = Data::SourceDelivery::Waiting();
  std::optional<Data::SourceDelivery::Answer> Answer;
  std::shared_ptr<const WeatherSnapshot> Snapshot;
  std::string Error;
  Phase Outcome = Phase::Acquiring;
  bool Abandoned = false;
};

WeatherSnapshotLoader::Work::Work(std::shared_ptr<Data::SourceSession> sources, int64_t utcS)
    : Sources(std::move(sources)),
      Query(Sources->Sources().Ask(
          Data::SourceRequest(Data::DataKind::Weather, Data::Address::Whole(0)))),
      UtcS(utcS) {}

WeatherSnapshotLoader::WeatherSnapshotLoader(std::shared_ptr<Data::SourceSession> sources,
                                             Data::SourceTransport &transport,
                                             Workers workers,
                                             int64_t utcS)
    : Transport_(transport),
      Io_(workers.Io),
      Compute_(workers.Compute),
      Work_(std::make_unique<Work>(std::move(sources), utcS)) {}

WeatherSnapshotLoader::~WeatherSnapshotLoader() {
  Cancel();
  if (Running()) { ActivePool_->Wait(Handle_); }
  if (!Work_->Abandoned) {
    Abandon();
    Io_.Wait(Handle_);
  }
}

bool WeatherSnapshotLoader::AwaitProgress(double seconds) const {
  return Running() && ActivePool_->AwaitCompletion(Handle_, seconds);
}

void WeatherSnapshotLoader::Cancel() noexcept {
  Work_->Stopping.store(true, std::memory_order_relaxed);
  Snapshot_.reset();
  Phase_ = Phase::Cancelled;
}

void WeatherSnapshotLoader::Abandon() {
  Work *const work = Work_.get();
  Data::SourceTransport *const transport = &Transport_;
  ActivePool_ = &Io_;
  Handle_ = Io_.Post([work, transport] {
    Data::SourceSet::Abandon(work->Query, *transport);
    work->Answer.reset();
    work->Delivery = Data::SourceDelivery::Consumed();
    work->Snapshot.reset();
    work->Abandoned = true;
  });
}

void WeatherSnapshotLoader::Decode() {
  Work *const work = Work_.get();
  ActivePool_ = &Compute_;
  Handle_ = Compute_.Post([work] {
    if (work->Stopping.load(std::memory_order_relaxed)) { return; }
    for (size_t at = 0; at < work->Sources->Sources().Count(); ++at) {
      const auto &source = work->Sources->Sources().At(at);
      const auto &decl = source.Declaration();
      const auto &answer = *work->Answer;
      if (decl.Kind != Data::DataKind::Weather || decl.Id != answer.SourceId ||
          decl.Revision != answer.SourceRevision || Data::SourceKey(decl) != answer.SourceKey) {
        continue;
      }
      auto decoded = source.DecodeWeather(answer.Bytes, work->UtcS);
      if (!decoded) {
        work->Outcome = Phase::Failed;
        work->Error = "weather source could not decode its delivered payload";
      } else if (!*decoded) {
        work->Outcome = Phase::Missing;
      } else if ((**decoded).Values.ValidUtcS != work->UtcS) {
        work->Outcome = Phase::Failed;
        work->Error = "weather decoder returned a different UTC";
      } else {
        work->Snapshot = std::make_shared<const WeatherSnapshot>(std::move(**decoded));
        work->Outcome = Phase::Ready;
      }
      work->Answer.reset();
      return;
    }
    work->Outcome = Phase::Failed;
    work->Error = "weather delivery does not match a configured source";
  });
}

void WeatherSnapshotLoader::Publish() {
  Phase_ = Work_->Outcome;
  if (Phase_ == Phase::Ready) { Snapshot_ = std::move(Work_->Snapshot); }
  Error_ = std::move(Work_->Error);
}

WeatherSnapshotLoader::Phase WeatherSnapshotLoader::Advance() {
  if (Running()) {
    if (!ActivePool_->TakeCompletion(Handle_)) { return Phase_; }
    Handle_ = Tasks::kNoTask;
    ActivePool_ = nullptr;
  }
  if (Work_->Stopping.load(std::memory_order_relaxed)) {
    if (!Work_->Abandoned) { Abandon(); }
    return Phase_ = Phase::Cancelled;
  }
  if (Phase_ == Phase::Decoding) {
    Publish();
    return Phase_;
  }
  if (Phase_ != Phase::Acquiring) { return Phase_; }
  if (Work_->Delivery.Where() == Data::SourceDelivery::State::Delivered) {
    Work_->Answer = Work_->Delivery.Take();
    Phase_ = Phase::Decoding;
    Decode();
  } else if (Work_->Delivery.Where() == Data::SourceDelivery::State::Vacant ||
             Work_->Delivery.Where() == Data::SourceDelivery::State::Undeclared) {
    Phase_ = Phase::Missing;
  } else if (Work_->Delivery.Where() == Data::SourceDelivery::State::Refused) {
    Phase_ = Phase::Failed;
    Error_ = "weather source acquisition was refused";
  } else {
    Work *const work = Work_.get();
    Data::SourceTransport *const transport = &Transport_;
    ActivePool_ = &Io_;
    Handle_ = Io_.Post([work, transport] {
      if (!work->Stopping.load(std::memory_order_relaxed)) {
        work->Delivery = work->Sources->Sources().Collect(work->Query, *transport);
      }
    });
  }
  return Phase_;
}
}
