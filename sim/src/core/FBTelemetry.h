/* FlightBox — telemetry: periodically sampled state, a time series with a schema (see FBLog.h for the
 * discrete-event counterpart). Classes DECLARE they carry telemetry (FBTelemetrySource — the
 * Serializable analogue); emission is CENTRAL (FBTelemetryBus, the one caller of every source's
 * Sample). FBTelemetrySource lives in core/ so systems/render/world/fdm can implement it without
 * depending on app/ — the concrete sink (CSV file, …) is injected from app/, same split as FBLog.
 *
 * A row is built by CONCATENATION: each source pushes exactly as many string fields as it declared
 * channels, in the same order, so declaration/registration order IS column order — no string-keyed
 * lookup at sample time. */
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

/* The one emitter. Sources register once (borrowed pointers — the bus never owns a system, mirrors
 * FBWorld's unit registration). Start() builds the schema (a leading "t" channel, then every source in
 * registration order) and pushes the header; Tick(simTime) samples every source into one row. A null
 * sink makes Tick() a cheap no-op — the WASM boot leaves it unset (CLAUDE.md: "Bus läuft, Sink null =
 * billig"). */
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
