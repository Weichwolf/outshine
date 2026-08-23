#include <chrono>
#include <cstdio>
#include <string>

#include "Check.h"

#include "ScenarioRead.h"
#include "Xml.h"

using outshine::ReadScenarioInto;
using outshine::Scenario;
using outshine::Xml;

namespace {

[[nodiscard]] std::string Rows(int n) {
  std::string text = "<scenario name=\"crowd\"><instances>";
  for (int at = 0; at < n; ++at) {
    text += "<instance of=\"car\" id=\"c" + std::to_string(at) + "\" x=\"" +
            std::to_string(at) + "\" y=\"0\" z=\"0\"/>";
  }
  return text + "</instances></scenario>";
}

struct Reading {
  size_t Steps = 0;
  double Ms = 0.0;
};

[[nodiscard]] Reading Read(const std::string &text, size_t &instances) {
  Xml document;
  if (!document.Parse(text.c_str(), text.size())) { return Reading{}; }
  Scenario declared;
  std::string error;
  const auto from = std::chrono::steady_clock::now();
  const bool read = ReadScenarioInto(document, declared, error);
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
  if (!read) { std::printf("REFUSED %s\n", error.c_str()); }
  instances = read ? declared.Instances.size() : 0;
  return Reading{document.SiblingSteps(), ms};
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  constexpr int kRows = 16000;
  const std::string text = Rows(kRows);
  size_t instances = 0;
  const Reading reading = Read(text, instances);

  CHECK(instances == (size_t)kRows, "the reader read every declared instance");
  Note("rows declared", (double)kRows, "rows");
  Note("sibling steps", (double)reading.Steps, "steps");
  Note("steps per row", (double)reading.Steps / (double)kRows, "steps/row");
  Note("wall clock", reading.Ms, "ms");

  // one pass over the instances plus the shallow walks the singleton lookups do: the
  // per-row term is what this claim owns, and it is a CONSTANT, never the row count.
  // The index idiom cost 2*k steps PER ROW -- 512 million for these 16 000 rows, 1268.70 ms
  // (board:1758). A stopwatch would pass on a fast machine; the count cannot.
  constexpr double kStepsPerRowBound = 4.0;
  CHECK((double)reading.Steps <= kStepsPerRowBound * (double)kRows,
        "**THE READER IS LINEAR IN THE ROWS IT READS**: the sibling walk costs a bounded "
        "number of steps PER ROW, so a scenario of a city block's parked cars is read in "
        "one pass and not in a square");

  Covers("III.9 a scenario of k rows is read in O(k) sibling steps -- the reader walks each "
         "collection once, and the count is published so a regression is a number and not a "
         "stopwatch (board:1758)");
  return Report();
}
