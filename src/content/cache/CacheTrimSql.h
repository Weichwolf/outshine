#ifndef OUTSHINE_CONTENT_CACHE_CACHETRIMSQL_H
#define OUTSHINE_CONTENT_CACHE_CACHETRIMSQL_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace outshine::CacheTrimSql {

struct Database {
  sqlite3 *Handle = nullptr;
  bool Transaction = false;
  Database() = default;
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;
  ~Database();
};

struct Statement {
  sqlite3_stmt *Handle = nullptr;
  Statement() = default;
  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;
  ~Statement();
};

[[nodiscard]] bool Key(std::string_view key);
[[nodiscard]] std::expected<bool, std::string> Exists(const std::filesystem::path &path);
[[nodiscard]] std::expected<std::unique_ptr<Database>, std::string>
Open(const std::filesystem::path &path, bool apply);
[[nodiscard]] std::expected<void, std::string> Exec(sqlite3 *database, const char *sql);
[[nodiscard]] std::expected<void, std::string>
Prepare(sqlite3 *database, const char *sql, Statement &statement);
[[nodiscard]] std::expected<bool, std::string> HasUsage(sqlite3 *database);
[[nodiscard]] std::expected<void, std::string> Commit(Database &database, bool compact);
[[nodiscard]] std::expected<uint64_t, std::string> FileBytes(const std::filesystem::path &path);
[[nodiscard]] std::expected<std::unordered_map<std::string, int64_t>, std::string>
Usage(sqlite3 *database);

}
#endif
