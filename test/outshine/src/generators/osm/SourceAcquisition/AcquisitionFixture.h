#ifndef OUTSHINE_TEST_SOURCEACQUISITION_ACQUISITIONFIXTURE_H
#define OUTSHINE_TEST_SOURCEACQUISITION_ACQUISITIONFIXTURE_H

#include "Check.h"
#include "OsmSourceAcquisition.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace SourceAcquisitionFixture {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr auto kDeadline = std::chrono::seconds(3);
constexpr GeoCellId kCell{.Level = 9, .X = 269, .Y = 411};
enum class Hold { None, Io, Compiler };

struct Gate {
  Hold Stage = Hold::None;
  std::binary_semaphore Entered{0}, Release{0}, CompilerGone{0};
  std::atomic_uint Begins{0}, Polls{0}, Canceled{0}, Compiled{0};
  std::atomic_bool CompilerOffOwner{false}, CallerIo{false};
  const std::thread::id Owner = std::this_thread::get_id();
  std::weak_ptr<const SourceSnapshot> Input;
};

class ProbeTransport final : public SourceTransport {
public:
  explicit ProbeTransport(Gate &gate) : Gate_(gate) {}

  [[nodiscard]] FetchStart Begin(const std::string & /*uri*/) override {
    ++Gate_.Begins;
    Gate_.CallerIo = Gate_.CallerIo || std::this_thread::get_id() == Gate_.Owner;
    return Ticket{19};
  }

  [[nodiscard]] Data::Wire Collect(Ticket /*ticket*/) override {
    const auto poll = ++Gate_.Polls;
    Gate_.CallerIo = Gate_.CallerIo || std::this_thread::get_id() == Gate_.Owner;
    if (Gate_.Stage == Hold::Io) {
      if (poll == 1) {
        Gate_.Entered.release();
        Gate_.Release.acquire();
      }
      return Data::Wire::Working();
    }
    const std::string xml = "<osm version='0.6'><node id='1' lat='54.7' lon='9.3'/></osm>";
    return Data::Wire::Answered(200, std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(Ticket /*ticket*/) override {
    ++Gate_.Canceled;
    Gate_.CallerIo = Gate_.CallerIo || std::this_thread::get_id() == Gate_.Owner;
  }

private:
  Gate &Gate_;
};

class Product final : public CellProduct {
public:
  explicit Product(std::shared_ptr<const SourceSnapshot> source) : Source_(std::move(source)) {}

  [[nodiscard]] size_t StorageChargeBytes() const noexcept override {
    return Source_->StorageChargeBytes();
  }

private:
  std::shared_ptr<const SourceSnapshot> Source_;
};

class Compiler final : public CellCompiler {
public:
  explicit Compiler(Gate &gate) : Gate_(gate) {}

  ~Compiler() override {
    Gate_.CompilerOffOwner = std::this_thread::get_id() != Gate_.Owner;
    Gate_.CompilerGone.release();
  }

  [[nodiscard]] std::expected<std::shared_ptr<const CellProduct>, std::string>
  Compile(std::shared_ptr<const SourceSnapshot> source,
          const std::stop_token &stop) const override {
    ++Gate_.Compiled;
    Gate_.Input = source;
    if (Gate_.Stage == Hold::Compiler) {
      Gate_.Entered.release();
      Gate_.Release.acquire();
    }
    if (stop.stop_requested()) { return std::unexpected("held compiler canceled"); }
    return std::make_shared<Product>(std::move(source));
  }

private:
  Gate &Gate_;
};

class WorkerBlock {
public:
  WorkerBlock() = default;

  ~WorkerBlock() { Release(); }

  WorkerBlock(const WorkerBlock &) = delete;
  WorkerBlock &operator=(const WorkerBlock &) = delete;

  [[nodiscard]] bool Start(Tasks &pool) {
    Signal_ = std::make_shared<Signal>();
    if (!pool.PostDetached([signal = Signal_] {
          signal->Entered.release();
          signal->Release.acquire();
        })) {
      Signal_.reset();
      return false;
    }
    return Signal_->Entered.try_acquire_for(kDeadline);
  }

  void Release() {
    if (const auto signal = std::exchange(Signal_, {})) { signal->Release.release(); }
  }

private:
  struct Signal {
    std::binary_semaphore Entered{0}, Release{0};
  };

  std::shared_ptr<Signal> Signal_;
};

inline std::expected<void, std::string> Start(SourceAcquisition &source, bool cells) {
  SourceProvider provider{.Kind = "osm",
                          .Revision = "held-source",
                          .Missing = MissingDataPolicy::Fail,
                          .Dataset = "openstreetmap.original",
                          .Location = {},
                          .Endpoint = "https://api.openstreetmap.org/api/0.6",
                          .Coverage = {},
                          .PayloadSha256 = {},
                          .Schema = {}};
  if (cells) {
    return source.RequestCells(provider,
                               std::array{kCell},
                               {.CellsMost = 2, .SnapshotBytesMost = size_t{1024} * 1024},
                               ".");
  }
  provider.Coverage = kCell.Bounds();
  return source.Request(std::array{provider}, ".");
}

inline void Barrier(Tasks &pool) {
  const auto task = pool.Post([] {});
  CHECK(task != Tasks::kNoTask, "the pool accepts a terminal barrier");
  if (task != Tasks::kNoTask) { pool.Wait(task); }
}
}
#endif
