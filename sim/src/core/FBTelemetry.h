/* Periodically sampled state, a time series with a schema (FBLog.h is the discrete-event counterpart).
 * Classes DECLARE themselves a source; emission is CENTRAL, and the concrete sink is injected from
 * app/. A row is built by CONCATENATION, so declaration/registration order IS column order — no
 * string-keyed lookup at sample time. doc/flightbox/core.md, Abschnitt 3.2. */
#ifndef FBTELEMETRY_H
#define FBTELEMETRY_H
#include <string>
#include <vector>

namespace FlightBox {

struct FBTelemetryChannel {
  std::string Name;
  std::string Unit;
};

class FBTelemetrySchema {
public:
  void Add(const std::string &name, const std::string &unit = "") { Channels_.push_back({name, unit}); }
  const std::vector<FBTelemetryChannel> &Channels() const { return Channels_; }

private:
  std::vector<FBTelemetryChannel> Channels_;
};

class FBTelemetryRow {
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

class FBTelemetrySource {
public:
  virtual ~FBTelemetrySource() = default;
  virtual const char *TelemetryName() const = 0;
  virtual void DeclareTelemetry(FBTelemetrySchema &schema) const = 0;   /* once, at Bus::Start() */
  virtual void SampleTelemetry(FBTelemetryRow &row) const = 0;          /* once per Bus::Tick() */
};

class FBTelemetrySink {
public:
  virtual ~FBTelemetrySink() = default;
  virtual void Header(const std::vector<std::string> &columns) = 0;
  virtual void Row(const std::vector<std::string> &fields) = 0;
};

/* The one emitter. Sources register once as BORROWED pointers — the bus never owns a system. A null
 * sink makes Tick() a cheap no-op, which is what the WASM boot leaves it as. */
class FBTelemetryBus {
public:
  void Register(FBTelemetrySource *src) { Sources_.push_back(src); }
  void SetSink(FBTelemetrySink *sink) { Sink_ = sink; }
  void Start();
  void Tick(double simTimeS);

private:
  std::vector<FBTelemetrySource *> Sources_;
  FBTelemetrySchema Schema_;
  FBTelemetryRow Row_;
  FBTelemetrySink *Sink_ = nullptr;
  bool Started_ = false;
};

} // namespace FlightBox
#endif /* FBTELEMETRY_H */
