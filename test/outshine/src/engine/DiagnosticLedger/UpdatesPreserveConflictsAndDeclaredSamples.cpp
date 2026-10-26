#include "DiagnosticLedger.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine::Test;
  outshine::Core::DiagnosticLedger ledger;
  ledger.ConfigureMetrics({{"declared", 42.0, "ms"}});
  CHECK(!ledger.RecordedThisFrame("declared") && !ledger.RecordedThisFrame("absent"),
        "declared samples are not mistaken for a current capture");
  ledger.BeginFrame();
  ledger.RecordMetric("declared", 1.0, "calls");
  CHECK(ledger.RecordedThisFrame("declared"), "an owned current sample permits lazy collection");
  for (int at = 0; at < 2048; ++at) { ledger.RecordMetric(std::to_string(at), double(at), "ms"); }
  ledger.BeginFrame();
  CHECK(!ledger.RecordedThisFrame("declared") && !ledger.RecordedThisFrame("0"),
        "a new round requires a fresh capture despite retained values");
  for (int at = 0; at < 2048; ++at) {
    ledger.RecordMetric(std::to_string(at), double(at + 1), "ms");
  }
  CHECK(ledger.Samples().size() == 2050 && ledger.Samples()[0].Value == 42.0,
        "dynamic updates preserve declared samples and stable row ownership through growth");
  CHECK(ledger.RecordedThisFrame("0") && !ledger.RecordedThisFrame("missing"),
        "capture ownership survives name-map and sample-vector growth");
  ledger.RecordMetric("0", 2.0, "ms");
  ledger.RecordMetric("0", 3.0, "ms");
  CHECK(ledger.ConflictingMetricNames().size() == 1 && ledger.Samples()[2].Value == 1.0,
        "conflicting repeated writes are reported once and do not overwrite the first value");
  ledger.ConfigureMetrics({});
  CHECK(!ledger.RecordedThisFrame("0"), "reset discards previous round capture ownership");
  ledger.BeginFrame();
  ledger.RecordMetric("0", 7.0, "ms");
  CHECK(ledger.Samples().size() == 1 && ledger.Samples()[0].Value == 7.0 &&
            ledger.ConflictingMetricNames().empty(),
        "reconfiguration clears all old indices and conflicts");
  return Report();
}
