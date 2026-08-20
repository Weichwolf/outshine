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

  void Header(const std::vector<std::string> &columns) override;
  void Row(const std::vector<std::string> &fields) override;

private:
  void WriteRow(const std::vector<std::string> &fields);

  std::FILE *File_;
};

}
#endif
