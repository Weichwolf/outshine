#include "DiagnosticLedger.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine::Test;
  outshine::Core::DiagnosticLedger ledger;
  ledger.ConfigureMetrics({{"declared", 42.0, "ms"}});
  ledger.BeginFrame();
  ledger.RecordMetric("declared", 1.0, "calls");
  for (int at = 0; at < 2048; ++at) { ledger.RecordMetric(std::to_string(at), double(at), "ms"); }
  ledger.BeginFrame();
  for (int at = 0; at < 2048; ++at) {
    ledger.RecordMetric(std::to_string(at), double(at + 1), "ms");
  }
  CHECK(ledger.Samples().size() == 2050 && ledger.Samples()[0].Value == 42.0,
        "dynamic updates preserve declared samples and stable row ownership through growth");
  ledger.RecordMetric("0", 2.0, "ms");
  ledger.RecordMetric("0", 3.0, "ms");
  CHECK(ledger.ConflictingMetricNames().size() == 1 && ledger.Samples()[2].Value == 1.0,
        "conflicting repeated writes are reported once and do not overwrite the first value");
  ledger.ConfigureMetrics({});
  ledger.BeginFrame();
  ledger.RecordMetric("0", 7.0, "ms");
  CHECK(ledger.Samples().size() == 1 && ledger.Samples()[0].Value == 7.0 &&
            ledger.ConflictingMetricNames().empty(),
        "reconfiguration clears all old indices and conflicts");
  return Report();
}
