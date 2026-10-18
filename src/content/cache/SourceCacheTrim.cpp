#include "CacheTrim.h"
#include "CacheTrimSql.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine {
namespace {
namespace Sql = CacheTrimSql;

struct Source {
  std::filesystem::path Path;
  std::filesystem::file_time_type Modified;
  uint64_t Bytes = 0;
  int64_t Used = 0;
  bool Absence = false;
};

struct SourceUsage {
  std::unique_ptr<Sql::Database> Index;
  std::unordered_map<std::string, int64_t> Entries;
};

std::expected<SourceUsage, std::string> OpenIndex(const std::filesystem::path &path, bool apply) {
  SourceUsage state;
  auto exists = Sql::Exists(path);
  if (!exists) { return std::unexpected(exists.error()); }
  if (!*exists) { return state; }
  auto opened = Sql::Open(path, apply);
  if (!opened) { return std::unexpected(opened.error()); }
  state.Index = std::move(*opened);
  auto hasUsage = Sql::HasUsage(state.Index->Handle);
  if (!hasUsage || !*hasUsage) { return std::unexpected("unsupported source usage index"); }
  auto observed = Sql::Usage(state.Index->Handle);
  if (!observed) { return std::unexpected(observed.error()); }
  state.Entries = std::move(*observed);
  return state;
}

std::expected<void, std::string> Collect(const std::filesystem::path &directory,
                                         const std::unordered_map<std::string, int64_t> &usage,
                                         bool absence,
                                         std::vector<Source> &sources,
                                         CacheTrimResult &result) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(directory, error);
  if (error == std::errc::no_such_file_or_directory) { return {}; }
  if (error) { return std::unexpected(directory.string() + ": " + error.message()); }
  if (!std::filesystem::is_directory(status)) {
    return std::unexpected(directory.string() + ": expected a cache directory, not a symlink");
  }
  for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end;
       it.increment(error)) {
    const auto key = it->path().filename().string();
    if (!Sql::Key(key)) { continue; }
    if (!std::filesystem::is_regular_file(it->symlink_status(error))) {
      if (error) { break; }
      continue;
    }
    const auto bytes = it->file_size(error);
    if (error) { break; }
    const auto modified = it->last_write_time(error);
    if (error) { break; }
    if (bytes > std::numeric_limits<uint64_t>::max() - result.StoredBytes) {
      return std::unexpected("source cache byte count overflow");
    }
    const auto observed = absence ? usage.end() : usage.find(key);
    const bool unknown = observed == usage.end();
    result.UnknownUse += unknown ? 1u : 0u;
    const auto fallback = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::file_clock::to_sys(modified).time_since_epoch())
                              .count();
    sources.push_back({.Path = it->path(),
                       .Modified = modified,
                       .Bytes = bytes,
                       .Used = unknown ? fallback : observed->second,
                       .Absence = absence});
    result.StoredBytes += bytes;
  }
  if (error) { return std::unexpected(directory.string() + ": " + error.message()); }
  return {};
}

std::expected<void, std::string> PruneUsage(const Sql::Database &database,
                                            const std::vector<Source> &sources) {
  if (auto created =
          Sql::Exec(database.Handle, "CREATE TEMP TABLE retained_sources(key TEXT PRIMARY KEY)");
      !created) {
    return created;
  }
  Sql::Statement insert;
  if (auto prepared =
          Sql::Prepare(database.Handle, "INSERT INTO retained_sources VALUES(?)", insert);
      !prepared) {
    return prepared;
  }
  for (const auto &source : sources) {
    if (source.Absence) { continue; }
    const auto key = source.Path.filename().string();
    if (sqlite3_bind_text(insert.Handle, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(insert.Handle) != SQLITE_DONE || sqlite3_reset(insert.Handle) != SQLITE_OK) {
      return std::unexpected(sqlite3_errmsg(database.Handle));
    }
  }
  return Sql::Exec(
      database.Handle,
      "DELETE FROM cache_usage_v1 WHERE key NOT IN (SELECT key FROM retained_sources)");
}

std::expected<void, std::string> Remove(const Source &source) {
  auto exists = Sql::Exists(source.Path);
  if (!exists) { return std::unexpected(exists.error()); }
  if (!*exists) { return std::unexpected("source cache changed during maintenance"); }
  std::error_code error;
  const auto modified = std::filesystem::last_write_time(source.Path, error);
  if (error) { return std::unexpected(error.message()); }
  const auto bytes = std::filesystem::file_size(source.Path, error);
  if (error) { return std::unexpected(error.message()); }
  if (modified != source.Modified || bytes != source.Bytes) {
    return std::unexpected("source cache changed during maintenance");
  }
  if (!std::filesystem::remove(source.Path, error) || error) {
    return std::unexpected(source.Path.string() + ": " + error.message());
  }
  return {};
}
}

std::expected<CacheTrimResult, std::string> TrimSourceCache(const std::filesystem::path &directory,
                                                            CacheTrimOptions options) {
  CacheTrimResult result;
  const auto path = directory / ".usage.sqlite";
  auto usage = OpenIndex(path, options.Apply);
  if (!usage) { return std::unexpected(usage.error()); }
  std::vector<Source> sources;
  for (const bool absence : {false, true}) {
    auto collected = Collect(absence ? directory / ".outshine-absence-v1" : directory,
                             usage->Entries,
                             absence,
                             sources,
                             result);
    if (!collected) { return std::unexpected(collected.error()); }
  }
  auto indexBytes = Sql::FileBytes(path);
  if (!indexBytes) { return std::unexpected(indexBytes.error()); }
  result.FileBytesBefore = result.StoredBytes + *indexBytes;
  std::ranges::sort(sources, [](const Source &left, const Source &right) {
    return left.Used != right.Used ? left.Used < right.Used : left.Path < right.Path;
  });
  result.RetainedBytes = result.StoredBytes;
  size_t selected = 0;
  for (const auto &source : sources) {
    if (options.BudgetBytes != 0 && result.RetainedBytes <= options.BudgetBytes) { break; }
    if (options.Apply) {
      if (auto removed = Remove(source); !removed) {
        return std::unexpected("source trim partially applied: " + removed.error());
      }
    }
    result.RetainedBytes -= source.Bytes;
    ++result.EntriesRemoved;
    ++selected;
  }
  result.FileBytesAfter = result.FileBytesBefore;
  if (!options.Apply) { return result; }
  sources.erase(sources.begin(), sources.begin() + static_cast<std::ptrdiff_t>(selected));
  if (usage->Index) {
    if (auto pruned = PruneUsage(*usage->Index, sources); !pruned) {
      return std::unexpected("source files trimmed; usage cleanup failed: " + pruned.error());
    }
    if (auto committed = Sql::Commit(*usage->Index, options.Compact); !committed) {
      return std::unexpected("source files trimmed; usage update failed: " + committed.error());
    }
  }
  usage->Index.reset();
  indexBytes = Sql::FileBytes(path);
  if (!indexBytes) { return std::unexpected(indexBytes.error()); }
  result.FileBytesAfter = result.RetainedBytes + *indexBytes;
  return result;
}

}
