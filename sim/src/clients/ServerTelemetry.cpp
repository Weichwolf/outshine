#include "ServerTelemetry.h"

#include "HttpPost.h"

namespace outshine::Clients {
namespace {

std::string Join(const std::vector<std::string> &v) {
  std::string out;
  for (size_t i = 0; i < v.size(); i++) {
    if (i) out += ',';
    out += v[i];
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
  if (Pending_.empty()) return;
  if (HttpPost(Url_, Pending_.data(), Pending_.size(), "text/csv")) Dropped_ = 0;
  else Dropped_++;
  Pending_.clear();
}

}  // namespace outshine::Clients
