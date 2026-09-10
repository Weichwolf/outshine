#include "io/LogSinks.h"
#include "Check.h"
#include <array>
#include <barrier>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto path = (std::filesystem::temp_directory_path() / "outshine-log-XXXXXX").string();
  const int descriptor = mkstemp(path.data());
  if (descriptor < 0) {
    CHECK(false, "temporary log file created");
    return Report();
  }
  close(descriptor);
  std::set<std::string> expected;
  constexpr size_t writers = 4, events = 64, fields = 32;
  for (size_t thread = 0; thread < writers; ++thread) {
    for (size_t event = 0; event < events; ++event) {
      const std::string identity = std::to_string(thread) + ":" + std::to_string(event);
      std::string line = "t=0.0 INFO world " + identity;
      for (size_t field = 0; field < fields; ++field) { line += " id=" + identity; }
      expected.insert(line);
    }
  }
  {
    const TextTarget target(path);
    TextLogSink sink(target);
    CHECK(target.File() != nullptr, "text target opened");
    std::barrier start(static_cast<std::ptrdiff_t>(writers));
    std::array<std::thread, writers> threads;
    for (size_t thread = 0; thread < writers; ++thread) {
      threads[thread] = std::thread([&, thread] {
        start.arrive_and_wait();
        for (size_t event = 0; event < events; ++event) {
          const std::string identity = std::to_string(thread) + ":" + std::to_string(event);
          const std::vector<LogField> values(fields, LogField("id", identity));
          sink.Write(0, LogLevel::Info, {.Tag = LogTag::World, .Event = identity.c_str()}, values);
        }
      });
    }
    for (auto &thread : threads) { thread.join(); }
  }
  std::ifstream input(path);
  std::string line;
  size_t unexpected = 0, count = 0;
  while (std::getline(input, line)) {
    ++count;
    if (expected.erase(line) != 1) { ++unexpected; }
  }
  CHECK(count == writers * events && unexpected == 0 && expected.empty(),
        "every concurrent event occupies one complete unique line");
  input.close();
  std::filesystem::remove(path);
  return Report();
}
