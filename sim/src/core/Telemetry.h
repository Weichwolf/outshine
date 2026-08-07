/* Periodically sampled state, a time series with a schema (Log.h is the discrete-event counterpart).
 * Classes DECLARE themselves a source; emission is CENTRAL, and the concrete sink is injected from
 * app/. A row is built by CONCATENATION, so declaration/registration order IS column order — no
 * string-keyed lookup at sample time. doc/core.md, Abschnitt 3.2. */
#ifndef TELEMETRY_H
#define TELEMETRY_H
#include <string>
#include <vector>

namespace outshine {

struct TelemetryChannel {
  std::string Name;
  std::string Unit;
};

class TelemetrySchema {
public:
  void Add(const std::string &name, const std::string &unit = "") { Channels_.push_back({name, unit}); }
  const std::vector<TelemetryChannel> &Channels() const { return Channels_; }

private:
  std::vector<TelemetryChannel> Channels_;
};

class TelemetryRow {
public:
  void Push(double v);
  void Push(int v);
  void Push(bool v);
  void Push(const std::string &v);
  const std::vector<std::string> &Fields() const { return Fields_; }
  void Clear() { Fields_.clear(); }

private:
  std::vector<std::string> Fields_;
};

class TelemetrySource {
public:
  virtual ~TelemetrySource() = default;
  virtual const char *TelemetryName() const = 0;
  virtual void DeclareTelemetry(TelemetrySchema &schema) const = 0;   /* once, at Bus::Start() */
  virtual void SampleTelemetry(TelemetryRow &row) const = 0;          /* once per Bus::Tick() */
};

class TelemetrySink {
public:
  virtual ~TelemetrySink() = default;
  virtual void Header(const std::vector<std::string> &columns) = 0;
  virtual void Row(const std::vector<std::string> &fields) = 0;
};

/* The one emitter. Sources register once as BORROWED pointers — the bus never owns a system. A null
 * sink makes Tick() a cheap no-op, which is what the WASM boot leaves it as. */
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
#endif /* TELEMETRY_H */
