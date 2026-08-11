/* THE LEDGER, one row per second on the same bus the frame spectrum rides. A fixed heap is a budget,
 * and a budget nobody measures against is a wish: this publishes what the client holds, what it has
 * ever held, and which pool holds it — so a rise has an owner instead of a suspicion.
 *
 * THE ROW CLOSES BY CONSTRUCTION: live bytes minus every pool column is published as the residual,
 * so what the ledger cannot attribute is a measurement of its own blind spot rather than a
 * subtraction the reader has to know to perform. The device's memory is a separate allocation path
 * and is reported beside the heap, never added to it. */
#ifndef MEMORYTELEMETRY_H
#define MEMORYTELEMETRY_H

#include "Renderer.h"
#include "Sim.h"
#include "Telemetry.h"
#include "World.h"

namespace outshine::Clients {

class MemoryTelemetry : public TelemetrySource {
public:
  /* The three that hold something: the world's streams, the renderer's device pools, and the
   * buffers the regions are generated into. Borrowed — the ledger owns nothing it counts. */
  MemoryTelemetry(const World::World &world, const Render::Renderer &renderer, const Sim &sim)
      : World_(world), Renderer_(renderer), Sim_(sim) {}

  const char *TelemetryName() const override { return "memory"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  void PushHeap(TelemetryRow &row, size_t live) const;
  void PushPools(TelemetryRow &row, const World::World::Pools &pools, size_t generator) const;
  void PushDevice(TelemetryRow &row) const;

  const World::World &World_;
  const Render::Renderer &Renderer_;
  const Sim &Sim_;
};

} // namespace outshine::Clients
#endif
