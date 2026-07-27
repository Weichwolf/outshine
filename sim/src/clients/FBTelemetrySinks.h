/* The telemetry sink implementations: app/ owns the file I/O, core/ only declares and samples. */
#ifndef FBTELEMETRYSINKS_H
#define FBTELEMETRYSINKS_H
#include <cstdio>
#include <string>
#include <vector>
#include "FBTelemetry.h"

namespace FlightBox::Clients {

/* One fopen'd FILE* for the run; the header is the schema-generated column list, joined once. */
class FBCsvTelemetrySink : public FBTelemetrySink {
public:
  explicit FBCsvTelemetrySink(FILE *f) : F(f) {}
  void Header(const std::vector<std::string> &columns) override;
  void Row(const std::vector<std::string> &fields) override;

private:
  FILE *F;
};

} // namespace FlightBox::Clients
#endif /* FBTELEMETRYSINKS_H */
