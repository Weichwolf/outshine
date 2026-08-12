#include "ServerTelemetry.h"

#include "HttpPost.h"

namespace outshine::Clients {
namespace {

/* RFC 4180. A browser's own version string carries a comma, and an unquoted one shifts every column
 * to its right by one for the whole run — the row is then unreadable by name. */
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

std::string Join(const std::vector<std::string> &v) {
  std::string out;
  for (size_t i = 0; i < v.size(); i++) {
    if (i) out += ',';
    Append(out, v[i]);
  }
  out += '\n';
  return out;
}

}  // namespace

ServerTelemetry::~ServerTelemetry() { Flush(); }

void ServerTelemetry::Header(const std::vector<std::string> &columns) {
  Pending_ += Join(columns);
}

void ServerTelemetry::Row(const std::vector<std::string> &fields) {
  Pending_ += Join(fields);
  if (Pending_.size() >= kFlushBytes) Flush();
}

void ServerTelemetry::Flush() {
  switch (Post_.Take()) {
    case HttpPost::State::Delivered: Dropped_ = 0; break;
    case HttpPost::State::Refused: Dropped_++; break;
    case HttpPost::State::InFlight: return;
    case HttpPost::State::Idle: break;
  }
  if (Pending_.empty()) return;
  Post_.Begin(Url_, Pending_.data(), Pending_.size(), "text/csv");
  Pending_.clear();
}

bool ServerTelemetry::Owed() const {
  return Post_.Ask() == HttpPost::State::InFlight || !Pending_.empty();
}

}  // namespace outshine::Clients
