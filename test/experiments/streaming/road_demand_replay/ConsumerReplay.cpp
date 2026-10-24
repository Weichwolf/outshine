#include "EngineState.h"
#include "ConsumerHooks.h"
#include <Engine.h>
#include <SDL3/SDL_stdinc.h>
#include <nlohmann/json.hpp>
#include <xxhash.h>
#include <zstd.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using ProbeJson = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr int Failure = 77;
std::unordered_set<std::string> Allowed;
std::mutex DemandMutex;
std::atomic<size_t> PayloadReads{0};
std::atomic<size_t> FineAssetLoads{0};
std::atomic<size_t> NativeBytes{0};
std::atomic<size_t> DeclaredSliceBytes{0};
std::atomic<size_t> ForbiddenReads{0};
std::atomic<size_t> Resolves{0};
size_t Frames = 0;
size_t ManifestCount = 0;
double CaptureMs = 0;
ProbeJson FrameRows = ProbeJson::array();
ProbeJson DemandRows = ProbeJson::array();

std::filesystem::path Directory() {
  const char *const path = SDL_getenv("OUTSHINE_CONSUMER_PROBE_DIR");
  if (path == nullptr) { std::_Exit(Failure); }
  return path;
}

void Write(const std::filesystem::path &path, std::span<const uint8_t> bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  if (!file) { std::_Exit(Failure); }
}

void Record(const std::filesystem::path &path, const ProbeJson &value) {
  const auto text = value.dump(2);
  Write(path, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
}

bool IsAllowed(std::string_view key) {
  const std::scoped_lock lock(DemandMutex);
  return Allowed.contains(std::string(key));
}

void RecordManifest(const outshine::Generators::RoadPageManifest &manifest,
                    const std::string &key) {
  auto pages = ProbeJson::array();
  for (size_t index = 0; index < manifest.Pages.size(); ++index) {
    pages.push_back({{"index", index}, {"key", manifest.Pages[index].Key}});
  }
  Record(Directory() / "consumer-manifest.json",
         ProbeJson{{"manifest_key", key}, {"pages", pages}});
}

struct Summary {
  ~Summary() {
    Record(Directory() / "consumer-summary.json",
           ProbeJson{
               {"frames", Frames},
               {"manifest_count", ManifestCount},
               {"road_load_key_calls", PayloadReads.load()},
               {"road_resolve_calls", Resolves.load()},
               {"road_asset_load_calls", FineAssetLoads.load()},
               {"native_bytes", NativeBytes.load()},
               {"declared_asset_slice_bytes", DeclaredSliceBytes.load()},
               {"forbidden_reads", ForbiddenReads.load()},
               {"capture_ms", CaptureMs},
               {"frame_rows", FrameRows},
               {"demands", DemandRows},
               {"qualified_for_page_suppression", false},
               {"scope",
                "Fixed-pose diagnostic candidate playback; functional, reflection and changed-root "
                "contracts remain unresolved. Readback times are not budget evidence."}});
  }
} Report;
}

namespace outshine {
class TestEngine {
public:
  static Result Snapshot(Engine &engine) {
    const auto began = Clock::now();
    auto &device = engine.S_->Picture.Device;
    std::vector<uint8_t> pixels;
    if (device.ReadPixels(pixels) != Render::ReadState::Ready) {
      return std::unexpected("consumer colour readback failed");
    }
    const auto hash = XXH3_128bits(pixels.data(), pixels.size());
    const auto name =
        std::format("frame-{}-{:016x}{:016x}.rgba.zst", Frames, hash.high64, hash.low64);
    std::vector<uint8_t> encoded(ZSTD_compressBound(pixels.size()));
    const size_t size =
        ZSTD_compress(encoded.data(), encoded.size(), pixels.data(), pixels.size(), 1);
    if (ZSTD_isError(size) != 0) { return std::unexpected("consumer colour compression failed"); }
    encoded.resize(size);
    Write(Directory() / name, encoded);
    FrameRows.push_back(ProbeJson{{"frame", Frames},
                                  {"serial", device.LastSubmittedCamera().Serial},
                                  {"width", device.SceneW()},
                                  {"height", device.SceneH()},
                                  {"raw_bytes", pixels.size()},
                                  {"file", name}});
    ++Frames;
    CaptureMs += std::chrono::duration<double, std::milli>(Clock::now() - began).count();
    return {};
  }
};
}

BeginResult ProbeBegin(outshine::Tasks &pool,
                       std::shared_ptr<outshine::Generators::PreparedRoadPages> pages,
                       std::shared_ptr<const outshine::Generators::RoadPageManifest> manifest,
                       std::optional<std::span<const uint32_t>> selected,
                       std::string key,
                       Restore::Limits limits) {
  const std::scoped_lock lock(DemandMutex);
  if (!manifest) { return std::unexpected("consumer manifest is missing"); }
  if (ManifestCount != 0) { return std::unexpected("consumer requires one native manifest"); }
  RecordManifest(*manifest, key);
  const char *const path = SDL_getenv("OUTSHINE_CONSUMER_SELECTION");
  if (path == nullptr) { return std::unexpected("consumer requires one explicit selection"); }
  std::ifstream file(path);
  const auto selection = ProbeJson::parse(file, nullptr, false);
  if (selection.is_discarded() || !selection.is_object() || !selection.contains("manifest_key") ||
      !selection["manifest_key"].is_string() || selection["manifest_key"] != key ||
      !selection.contains("candidate_indices") || !selection["candidate_indices"].is_array()) {
    return std::unexpected("consumer manifest or selection schema differs");
  }
  std::vector<uint32_t> indices;
  std::unordered_set<std::string> allowed;
  indices.reserve(selection["candidate_indices"].size());
  for (const auto &value : selection["candidate_indices"]) {
    if (!value.is_number_unsigned() ||
        value.get<uint64_t>() > std::numeric_limits<uint32_t>::max()) {
      return std::unexpected("consumer selection requires unsigned 32-bit indices");
    }
    const auto index = value.get<uint32_t>();
    if (index >= manifest->Pages.size() || !allowed.insert(manifest->Pages[index].Key).second) {
      return std::unexpected("consumer selection index invalid or repeated");
    }
    indices.push_back(index);
  }
  auto demand = ProbeJson{{"manifest_key", key},
                          {"all_pages", manifest->Pages.size()},
                          {"selected_indices", indices},
                          {"prior_selection_explicit", selected.has_value()}};
  auto begun = RealBegin(pool,
                         std::move(pages),
                         std::move(manifest),
                         std::span<const uint32_t>(indices),
                         std::move(key),
                         limits);
  if (!begun) { return begun; }
  Allowed = std::move(allowed);
  DemandRows.push_back(std::move(demand));
  ++ManifestCount;
  return begun;
}

LoadResult
ProbeLoad(outshine::Generators::PreparedRoadPages *pages, const std::string &key, size_t limit) {
  if (!IsAllowed(key)) {
    ++ForbiddenReads;
    return std::unexpected("consumer forbids excluded payload read");
  }
  ++PayloadReads;
  auto loaded = RealLoad(pages, key, limit);
  if (loaded && *loaded) { NativeBytes += (**loaded).NativeBytes; }
  return loaded;
}

ResolveResult ProbeResolve(outshine::Generators::PreparedRoadPages *pages,
                           const outshine::Generators::RoadPageRequest &request,
                           const outshine::Generators::RoadPageProducer *producer,
                           [[maybe_unused]] std::stop_token stop) {
  ++Resolves;
  return std::unexpected(std::format(
      "consumer forbids resolution: cache={} level={} parent={} request={} limit={} producer={}",
      pages != nullptr,
      request.Address.Level,
      request.Parent,
      request.RequestKey,
      request.NativeBytesMost,
      producer != nullptr));
}

CacheResult ProbeCacheLoad(const outshine::AssetCache *cache, std::string_view key, size_t limit) {
  auto metadata = cache->Find(key);
  if (!metadata) { return std::unexpected(metadata.error()); }
  if (*metadata && (**metadata).Kind == "road-render-page") {
    if (!IsAllowed(key)) {
      ++ForbiddenReads;
      return std::unexpected(outshine::AssetCacheError::InvalidInput);
    }
    ++FineAssetLoads;
    DeclaredSliceBytes += (**metadata).ByteCount;
  }
  return RealCacheLoad(cache, key, limit);
}

outshine::Result ProbeRender(outshine::Engine *engine, outshine::Extent extent) {
  auto result = RealRender(engine, extent);
  if (!result) { return result; }
  return outshine::TestEngine::Snapshot(*engine);
}
