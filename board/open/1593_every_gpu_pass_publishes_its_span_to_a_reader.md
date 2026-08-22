Type: task
Parent: 1571
Area: render
Tags: instrument

**Every GPU pass publishes its span to a reader**

The orphaned half of 0058/0099, refiled at their closing: the deleted GpuTimer measured per-pass
GPU time that nothing consumed. The living need is the pair -- a per-pass span measured on the
device AND a consumer (a telemetry row, a frame-attribution line like board:1571's WORST print)
-- so a cost that moves between passes is attributable on the A18 Pro without a profiler
attached. An instrument nothing reads is deleted on sight; this item exists to build the reader
first and the probe second.
