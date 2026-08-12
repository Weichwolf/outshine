#ifndef CSVTELEMETRY_H
#define CSVTELEMETRY_H

#include <cstdio>
#include <string>
#include <vector>

#include "Telemetry.h"
#include "TextTarget.h"

namespace outshine::Clients {

/* THE STATE CHANNEL AS TEXT, one CSV per run where the consumer said. A row is emitted once a
 * second, so the flush per row costs nothing a frame can see and no row can be lost to a buffer a
 * killed run never emptied. */
class CsvTelemetry : public TelemetrySink {
public:
  explicit CsvTelemetry(const TextTarget &target) : File_(target.File()) {}

  void Header(const std::vector<std::string> &columns) override;
  void Row(const std::vector<std::string> &fields) override;

private:
  void WriteRow(const std::vector<std::string> &fields);

  std::FILE *File_;   /* borrowed from the target, which outlives this sink */
};

} // namespace outshine::Clients
#endif
