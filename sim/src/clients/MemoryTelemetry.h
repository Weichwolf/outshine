/* THE LEDGER, one row per second on the same bus the frame spectrum rides. A fixed heap is a budget,
 * and a budget nobody measures against is a wish: this publishes what the client holds, what it has
 * ever held, and which pool holds it — so a rise has an owner instead of a suspicion.
 *
 * WHAT THIS ROW DOES NOT COVER, because a ledger that hides its edges is worse than none. The heap
 * here is THIS module's linear memory and no other. In the browser the tile pool runs N further wasm
 * modules, each with a linear memory of its own that grows, and the client's footprint is the sum —
 * the pool announces N when it opens. The device's memory is a separate allocation path and is
 * reported beside the heap, never added to it. */
#ifndef MEMORYTELEMETRY_H
#define MEMORYTELEMETRY_H

#include "Forest.h"
#include "Renderer.h"
#include "Telemetry.h"
#include "World.h"

namespace outshine::Clients {

class MemoryTelemetry : public TelemetrySource {
public:
  /* The three that hold something: the world's streams, the renderer's device pools, and the forest
   * grown once at bring-up. Borrowed — the ledger owns nothing it counts. */
  MemoryTelemetry(const World::World &world, const Render::Renderer &renderer,
                  const World::Forest &forest)
      : World_(world), Renderer_(renderer), Forest_(forest) {}

  const char *TelemetryName() const override { return "memory"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  const World::World &World_;
  const Render::Renderer &Renderer_;
  const World::Forest &Forest_;
};

} // namespace outshine::Clients
#endif
