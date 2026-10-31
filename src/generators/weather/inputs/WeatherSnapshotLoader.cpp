#include "WeatherSnapshotLoader.h"
#include "SourceSession.h"
#include <DataKind.h>
#include <SourceRequest.h>
#include <utility>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace outshine::Generators::Weather {
struct WeatherSnapshotLoader::Work {
  struct Job {
    explicit Job(std::shared_ptr<Work> owner) : Owner(std::move(owner)) {
      const std::scoped_lock lock(Owner->Mutex);
      Owner->Busy = true;
    }

    ~Job() {
      {
        const std::scoped_lock lock(Owner->Mutex);
        Owner->Busy = false;
      }
      Owner->Completed.notify_all();
    }

    Job(const Job &) = delete;
    Job &operator=(const Job &) = delete;
    std::shared_ptr<Work> Owner;
  };

  void Decode();
  void Poll();
  void Abandon();
  std::mutex Mutex;
  std::condition_variable Completed;
  bool Busy = false;
  Work(std::shared_ptr<Data::SourceSession> sources,
       Data::SourceTransport &transport,
       int64_t utcS);
  std::shared_ptr<Data::SourceSession> Sources;
  std::optional<Data::SourceSession::Read> Input;
  int64_t UtcS;
  std::atomic_bool Stopping = false;
  Data::SourceDelivery Delivery = Data::SourceDelivery::Waiting();
  std::optional<Data::SourceDelivery::Answer> Answer;
  std::shared_ptr<const WeatherSnapshot> Snapshot;
  std::string Error;
  Phase Outcome = Phase::Acquiring;
  bool Abandoned = false;
};

WeatherSnapshotLoader::Work::Work(std::shared_ptr<Data::SourceSession> sources,
                                  Data::SourceTransport &transport,
                                  int64_t utcS)
    : Sources(std::move(sources)), UtcS(utcS) {
  auto read = Data::SourceSession::Request(
      Sources, Data::SourceRequest(Data::DataKind::Weather, Data::Address::Whole(0)), transport);
  if (!read) {
    Error = std::move(read.error());
    Outcome = Phase::Failed;
    Abandoned = true;
    return;
  }
  Input.emplace(std::move(*read));
}

WeatherSnapshotLoader::WeatherSnapshotLoader(std::shared_ptr<Data::SourceSession> sources,
                                             Data::SourceTransport &transport,
                                             Workers workers,
                                             int64_t utcS)
    : Io_(workers.Io),
      Compute_(workers.Compute),
      Work_(std::make_shared<Work>(std::move(sources), transport, utcS)) {
  if (!Work_->Input) {
    Error_ = Work_->Error;
    Phase_ = Phase::Failed;
  }
}

WeatherSnapshotLoader::~WeatherSnapshotLoader() {
  Cancel();
  (void)Io_.PostDetached([work = std::move(Work_)] { (void)work; });
}

bool WeatherSnapshotLoader::Running() const noexcept {
  const std::scoped_lock lock(Work_->Mutex);
  return Work_->Busy;
}

bool WeatherSnapshotLoader::AwaitProgress(double seconds) const {
  if (!(seconds > 0.0)) { return false; }
  std::unique_lock lock(Work_->Mutex);
  return Work_->Busy && Work_->Completed.wait_for(lock,
                                                  std::chrono::duration<double>(seconds),
                                                  [this] { return !Work_->Busy; });
}

void WeatherSnapshotLoader::Schedule(Tasks &pool, void (Work::*execute)()) {
  const auto job = std::make_shared<Work::Job>(Work_);
  if (!pool.PostDetached([job, execute] { ((*job->Owner).*execute)(); })) {
    Phase_ = Phase_ == Phase::Cancelled ? Phase::Cancelled : Phase::Failed;
    Error_ = "weather worker pool is stopping";
  }
}

void WeatherSnapshotLoader::Cancel() noexcept {
  Work_->Stopping.store(true, std::memory_order_relaxed);
  Snapshot_.reset();
  Phase_ = Phase::Cancelled;
}

void WeatherSnapshotLoader::Work::Abandon() {
  if (Input) { Input->Cancel(); }
  Input.reset();
  Answer.reset();
  Delivery = Data::SourceDelivery::Consumed();
  Snapshot.reset();
  Abandoned = true;
}

void WeatherSnapshotLoader::Work::Poll() {
  if (!Stopping.load(std::memory_order_relaxed) && Input) { Delivery = Input->Poll(); }
}

void WeatherSnapshotLoader::Work::Decode() {
  if (Stopping.load(std::memory_order_relaxed)) { return; }
  if (!Answer) {
    Outcome = Phase::Failed;
    Error = "weather decoder requires delivered source bytes";
    return;
  }
  for (size_t at = 0; at < Sources->Sources().Count(); ++at) {
    const auto &source = Sources->Sources().At(at);
    const auto &decl = source.Declaration();
    const auto &answer = *Answer;
    if (decl.Kind != Data::DataKind::Weather || decl.Id != answer.SourceId ||
        decl.Revision != answer.SourceRevision || Data::SourceKey(decl) != answer.SourceKey) {
      continue;
    }
    auto decoded = source.DecodeWeather(answer.Bytes, UtcS);
    if (!decoded) {
      Outcome = Phase::Failed;
      Error = "weather source could not decode its delivered payload";
    } else if (!*decoded) {
      Outcome = Phase::Missing;
    } else if ((**decoded).Values.ValidUtcS != UtcS) {
      Outcome = Phase::Failed;
      Error = "weather decoder returned a different UTC";
    } else {
      Snapshot = std::make_shared<const WeatherSnapshot>(std::move(**decoded));
      Outcome = Phase::Ready;
    }
    Answer.reset();
    return;
  }
  Outcome = Phase::Failed;
  Error = "weather delivery does not match a configured source";
}

void WeatherSnapshotLoader::Publish() {
  Phase_ = Work_->Outcome;
  if (Phase_ == Phase::Ready) { Snapshot_ = std::move(Work_->Snapshot); }
  Error_ = std::move(Work_->Error);
}

WeatherSnapshotLoader::Phase WeatherSnapshotLoader::Advance() {
  if (Running()) { return Phase_; }
  if (Work_->Stopping.load(std::memory_order_relaxed)) {
    if (!Work_->Abandoned) { Schedule(Io_, &Work::Abandon); }
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
    Schedule(Compute_, &Work::Decode);
  } else if (Work_->Delivery.Where() == Data::SourceDelivery::State::Vacant ||
             Work_->Delivery.Where() == Data::SourceDelivery::State::Undeclared) {
    Phase_ = Phase::Missing;
  } else if (Work_->Delivery.Where() == Data::SourceDelivery::State::Refused) {
    Phase_ = Phase::Failed;
    Error_ = "weather source acquisition was refused";
  } else {
    Schedule(Io_, &Work::Poll);
  }
  return Phase_;
}
}
