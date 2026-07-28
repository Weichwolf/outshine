#include "FBTelemetrySinks.h"

namespace FlightBox::Clients {

namespace {
void WriteJoined(FILE *f, const std::vector<std::string> &fields) {
  for (size_t i = 0; i < fields.size(); i++) {
    if (i) fputc(',', f);
    fputs(fields[i].c_str(), f);
  }
  fputc('\n', f);
  fflush(f);
}
} // namespace

void FBCsvTelemetrySink::Header(const std::vector<std::string> &columns) {
  if (F) WriteJoined(F, columns);
}
void FBCsvTelemetrySink::Row(const std::vector<std::string> &fields) {
  if (F) WriteJoined(F, fields);
}

} // namespace FlightBox::Clients
