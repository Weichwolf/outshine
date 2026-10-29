#include "BuildingSourceCapture.h"
#include "BuildingSourceBudget.h"
#include "ReadTextFile.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <ratio>
#include <stop_token>
#include <string_view>

namespace {
using Json = nlohmann::json;
namespace Fs = std::filesystem;

[[noreturn]] void Fail(std::string_view message) {
  std::println(stderr, "{}", message);
  std::_Exit(1);
}

Json Document(const Fs::path &path) {
  const auto bytes = outshine::ReadTextFile(path.string(), size_t{1024} * 1024);
  if (!bytes) { Fail(bytes.error()); }
  const auto document = Json::parse(*bytes, nullptr, false);
  if (!document.is_object()) { Fail("object request required"); }
  return document;
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { Fail("expected original-input.json request.json new-output-directory"); }
  const Fs::path output(argv[3]);
  if (Fs::exists(output) || !Fs::create_directories(output)) {
    Fail("new output directory required");
  }
  const auto began = std::chrono::steady_clock::now();
  const auto request = Document(argv[2]);
  outshine::Test::BuildingSourceCapture source(argv[1], request);
  const auto &limits = request.at("limits");
  const outshine::Generators::BuildingSourceBudget budget{
      .Sources = limits.at("sources").get<uint64_t>(),
      .Faces = limits.at("faces").get<uint64_t>(),
      .TriangleAttempts = limits.at("triangle_attempts").get<uint64_t>()};
  std::stop_source stop;
  if (request.value("cancelled", false)) { stop.request_stop(); }
  const auto result = source.ExportOriginals(budget, output / "geometry" / "0", stop.get_token());
  Json report = source.Report();
  report["request"] = request;
  report["elapsed_ms"] =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  report["scope"] = "CPU original geometry from source headers and lazy forms; no GPU, cache, "
                    "World admission or peak-memory acceptance";
  std::ofstream file(output / "originals-result.json");
  file << report.dump(2) << '\n';
  if (!file) { Fail("report write failed"); }
  return result ? 0 : 1;
}
