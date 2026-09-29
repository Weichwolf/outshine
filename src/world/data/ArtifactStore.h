#ifndef OUTSHINE_WORLD_DATA_ARTIFACTSTORE_H
#define OUTSHINE_WORLD_DATA_ARTIFACTSTORE_H

#include "ArtifactBlocks.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Data {

class ArtifactStore {
public:
  struct Config {
    std::string Directory;
    size_t CapBytes = size_t{2} << 30;
  };

  class Reader {
  public:
    ~Reader();
    [[nodiscard]] const ArtifactManifest &Manifest() const noexcept;
    [[nodiscard]] std::optional<std::vector<uint8_t>> ReadBlock(std::string_view key, size_t bytes);

  private:
    friend class ArtifactStore;
    struct State;
    explicit Reader(std::unique_ptr<State> state);
    std::unique_ptr<State> State_;
  };

  class Writer {
  public:
    ~Writer();
    [[nodiscard]] bool Append(std::string_view key, std::span<const uint8_t> bytes);
    [[nodiscard]] bool Publish();

  private:
    friend class ArtifactStore;
    struct State;
    explicit Writer(std::unique_ptr<State> state);
    std::unique_ptr<State> State_;
  };

  explicit ArtifactStore(Config config);
  [[nodiscard]] bool Enabled() const noexcept;
  [[nodiscard]] const std::string &Directory() const noexcept;
  [[nodiscard]] std::unique_ptr<Reader> Read(std::string_view key, ArtifactLimits limits) const;
  [[nodiscard]] std::unique_ptr<Writer> Begin(std::string_view key, ArtifactLimits limits);
  [[nodiscard]] bool Trim() const;

private:
  Config Config_;
  std::atomic<uint64_t> Serial_{0};
};

}
#endif
