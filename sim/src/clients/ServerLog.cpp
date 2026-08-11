#include "ServerLog.h"

#include <cstdio>
#include <ctime>

#include "HttpPost.h"

namespace outshine::Clients {
namespace {

const char *LevelStr(LogLevel l) {
  switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "?";
}

/* The run id becomes a FILE NAME on the collector, so it is one alphabet and nothing else. */
std::string Sanitise(const std::string &s) {
  std::string out;
  for (char c : s) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    c == '-' || c == '_';
    out += ok ? c : '-';
  }
  return out.empty() ? std::string("run") : out;
}

}  // namespace

ServerLog::ServerLog(const std::string &base, const Identity &id) {
  char stamp[32];
  const time_t t = time(nullptr);
  struct tm g = {};
  gmtime_r(&t, &g);
  snprintf(stamp, sizeof stamp, "%04d%02d%02dT%02d%02d%02dZ", g.tm_year + 1900, g.tm_mon + 1,
           g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
  RunId_ = Sanitise(id.Mod) + "-" + Sanitise(id.Scene) + "-" + Sanitise(id.Client) + "-" + stamp;
  Url_ = base + "/log/" + RunId_;

  /* THE FIRST LINE NAMES THE RUN, so a log cut out of the directory still says what it is. */
  Pending_ = "t=0.0 INFO run identity mod=" + id.Mod + " scene=" + id.Scene +
             " client=" + id.Client + " build=" + (id.Build.empty() ? "unset" : id.Build) +
             " host=" + id.Host + " runId=" + RunId_ + "\n";
}

ServerLog::~ServerLog() { Flush(); }

void ServerLog::Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                      const std::vector<LogField> &fields) {
  char head[192];
  snprintf(head, sizeof head, "t=%.1f %s %s %s", simTimeS, LevelStr(level), tag, event);
  Pending_ += head;
  for (const LogField &f : fields) {
    Pending_ += ' ';
    Pending_ += f.Key;
    Pending_ += '=';
    if (f.Value.find(' ') != std::string::npos) Pending_ += '"' + f.Value + '"';
    else Pending_ += f.Value;
  }
  Pending_ += '\n';
  if (Pending_.size() >= kFlushBytes) Flush();
}

bool ServerLog::Flush() {
  switch (Post_.Take()) {
    case HttpPost::State::Delivered: Sent_++; Dropped_ = 0; break;
    case HttpPost::State::Refused: Dropped_++; break;
    case HttpPost::State::InFlight: return false;
    case HttpPost::State::Idle: break;
  }
  if (Pending_.empty()) return true;
  if (Dropped_) {
    char note[96];
    snprintf(note, sizeof note, "t=0.0 WARN run log_batches_lost batches=%zu\n", Dropped_);
    Pending_ += note;
  }
  Post_.Begin(Url_, Pending_.data(), Pending_.size(), "text/plain");
  Pending_.clear();
  return false;
}

}  // namespace outshine::Clients
