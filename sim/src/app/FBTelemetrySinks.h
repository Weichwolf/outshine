/* FlightBox — FBTelemetry sink implementations. app/ owns the file I/O (CLAUDE.md exception); core/
 * only ever declares/samples channels. */
#ifndef FBTELEMETRYSINKS_H
#define FBTELEMETRYSINKS_H
#include <cstdio>
#include <string>
#include <vector>
#include "FBTelemetry.h"

namespace FlightBox {

/* telemetry.csv — header is the schema-generated column list (comma-joined once), each row is the
 * comma-joined sampled fields; one fopen'd FILE* for the runner's lifetime. */
class FBCsvTelemetrySink : public FBTelemetrySink {
public:
  explicit FBCsvTelemetrySink(FILE *f) : F(f) {}
  void Header(const std::vector<std::string> &columns) override;
  void Row(const std::vector<std::string> &fields) override;

private:
  FILE *F;
};

} // namespace FlightBox
#endif /* FBTELEMETRYSINKS_H */
