#include "CsvTelemetry.h"

namespace outshine::Clients {
namespace {

void Append(std::string &out, const std::string &field) {
  if (field.find_first_of(",\"\n") == std::string::npos) {
    out += field;
    return;
  }
  out += '"';
  for (char c : field) {
    if (c == '"') out += '"';
    out += c;
  }
  out += '"';
}

}

void CsvTelemetry::WriteRow(std::span<const std::string> fields) {
  if (!File_) return;
  std::string line;
  for (size_t i = 0; i < fields.size(); i++) {
    if (i) line += ',';
    Append(line, fields[i]);
  }
  line += '\n';
  fwrite(line.data(), 1, line.size(), File_);
  fflush(File_);
}

void CsvTelemetry::Header(std::span<const std::string> columns) { WriteRow(columns); }

void CsvTelemetry::Row(std::span<const std::string> fields) { WriteRow(fields); }

}
