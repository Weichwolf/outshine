#ifndef CSVTELEMETRY_H
#define CSVTELEMETRY_H

#include <cstdio>
#include <string>
#include <vector>

#include "Telemetry.h"
#include "TextTarget.h"

namespace outshine::Clients {

class CsvTelemetry : public TelemetrySink {
public:
  explicit CsvTelemetry(const TextTarget &target) : File_(target.File()) {}

  void Header(std::span<const std::string> columns) override;
  void Row(std::span<const std::string> fields) override;

private:
  void WriteRow(std::span<const std::string> fields);

  std::FILE *File_;
};

}
#endif
