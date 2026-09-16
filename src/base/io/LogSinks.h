#ifndef OUTSHINE_BASE_IO_LOGSINKS_H
#define OUTSHINE_BASE_IO_LOGSINKS_H
#include <cstdio>
#include <mutex>
#include <vector>
#include "Logging.h"
#include "TextTarget.h"

namespace outshine {

class TextLogSink : public LogSink {
public:
  explicit TextLogSink(const TextTarget &target) : File_(target.File()) {}

  void
  Write(double simTimeS, LogLevel level, Saying who, std::span<const LogField> fields) override;

private:
  std::FILE *File_;
  std::mutex Mutex_;
};

}
#endif
