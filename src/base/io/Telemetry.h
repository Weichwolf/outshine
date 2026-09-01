#ifndef OUTSHINE_BASE_IO_TELEMETRY_H
#define OUTSHINE_BASE_IO_TELEMETRY_H
#include <span>
#include <string>
#include <vector>

namespace outshine {

struct TelemetryChannel {
  std::string Name;
  std::string Unit;
};

class TelemetrySchema {
public:
  void Add(const std::string &name, const std::string &unit = "") {
    Channels_.push_back({.Name = name, .Unit = unit});
  }

  [[nodiscard]] const std::vector<TelemetryChannel> &Channels() const { return Channels_; }

private:
  std::vector<TelemetryChannel> Channels_;
};

class TelemetryRow {
public:
  void Push(double v);
  void Push(int v);

  void Push(long long v);
  void Push(bool v);
  void Push(const std::string &v);

  [[nodiscard]] const std::vector<std::string> &Fields() const { return Fields_; }

  void Clear() { Fields_.clear(); }

private:
  std::vector<std::string> Fields_;
};

class TelemetrySource {
public:
  virtual ~TelemetrySource() = default;
  [[nodiscard]] virtual const char *TelemetryName() const = 0;
  virtual void DeclareTelemetry(TelemetrySchema &schema) const = 0;
  virtual void SampleTelemetry(TelemetryRow &row) const = 0;
};

class TelemetrySink {
public:
  virtual ~TelemetrySink() = default;
  virtual void Header(std::span<const std::string> columns) = 0;
  virtual void Row(std::span<const std::string> fields) = 0;
};

class TelemetryBus {
public:
  void Register(TelemetrySource *src) { Sources_.push_back(src); }

  void SetSink(TelemetrySink *sink) { Sink_ = sink; }

  void Start();
  void Tick(double simTimeS);

private:
  std::vector<TelemetrySource *> Sources_;
  TelemetrySchema Schema_;
  TelemetryRow Row_;
  TelemetrySink *Sink_ = nullptr;
  bool Started_ = false;
};

} // namespace outshine
#endif
