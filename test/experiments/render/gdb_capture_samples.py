"""GDB-only CPU-tick-triggered stacks of an unchanged native capture exporter.

Run through sample_building_capture.py, after GDB has stopped at main.
The worker never calls the GDB API. This is intrusive sampling, not perf or
instruction counting; original outputs must still pass independent comparison.
"""

import json
import os
import random
import signal
import threading
import time
from pathlib import Path

import gdb


def cpu_ticks(pid):
    fields = Path(f"/proc/{pid}/stat").read_text().rpartition(")")[2].split()
    return int(fields[11]) + int(fields[12])


class BakeEntry(gdb.Breakpoint):
    def __init__(self, pid, active, entries):
        super().__init__(
            "(anonymous namespace)::Bake(outshine::Generators::PreparedStructureTile const&, outshine::Generators::RawTile)",
            internal=True,
        )
        self.pid, self.active, self.entries = pid, active, entries
        self.finishes = []

    def stop(self):
        if self.active["phase"] != -1:
            raise RuntimeError(
                "nested Bake calls do not match this diagnostic contract"
            )
        index = len(self.entries)
        entry = {"index": index, "start_cpu_ticks": cpu_ticks(self.pid)}
        self.entries.append(entry)
        self.active["phase"] = index
        pid, active = self.pid, self.active

        class Finish(gdb.FinishBreakpoint):
            def stop(self):
                entry["finish_cpu_ticks"] = cpu_ticks(pid)
                active["phase"] = -1
                return False

            def out_of_scope(self):
                entry["unexpected_unwind"] = True

        self.finishes.append(Finish(gdb.newest_frame(), internal=True))
        return False


def sample(output):
    pid = gdb.selected_inferior().pid
    clock_hz = os.sysconf("SC_CLK_TCK")
    done, armed = threading.Event(), threading.Event()
    state = {"stop": None, "exit": None, "worker_error": None, "phase": -1}
    samples, entries = [], []
    entry_breakpoint = BakeEntry(pid, state, entries)
    rng = random.Random(12)
    started = time.monotonic()
    libraries = sorted(
        {
            line.split(maxsplit=5)[5]
            for line in Path(f"/proc/{pid}/maps").read_text().splitlines()
            if " r-xp " in line and len(line.split(maxsplit=5)) == 6
        }
    )
    pidfd = os.pidfd_open(pid)

    def stopped(event):
        state["stop"] = (
            event.stop_signal
            if isinstance(event, gdb.SignalEvent)
            else str(type(event))
        )

    def exited(event):
        state["exit"] = event.exit_code if hasattr(event, "exit_code") else -1
        done.set()

    def interrupt():
        try:
            while not done.is_set():
                if not armed.wait(0.01):
                    continue
                start = cpu_ticks(pid)
                target = start + max(1, round(rng.expovariate(1 / 2)))
                while not done.wait(0.002):
                    if cpu_ticks(pid) >= target:
                        armed.clear()
                        signal.pidfd_send_signal(pidfd, signal.SIGINT)
                        break
        except (FileNotFoundError, ProcessLookupError):
            pass
        except (OSError, ValueError) as error:
            state["worker_error"] = repr(error)

    gdb.events.stop.connect(stopped)
    gdb.events.exited.connect(exited)
    gdb.execute("handle SIGINT stop nopass", to_string=True)
    worker = threading.Thread(target=interrupt, daemon=True)
    with gdb.blocked_signals():
        worker.start()
    try:
        while not done.is_set():
            state["stop"] = None
            armed.set()
            gdb.execute("continue", to_string=True)
            armed.clear()
            if done.is_set():
                break
            if state["stop"] != "SIGINT":
                raise RuntimeError(f"unexpected native stop: {state['stop']}")
            if len(gdb.selected_inferior().threads()) != 1:
                raise RuntimeError("single-thread exporter required for this sampler")
            frame, stack = gdb.newest_frame(), []
            while frame is not None and len(stack) < 64:
                stack.append(frame.name() or f"unknown@{frame.pc():x}")
                frame = frame.older()
            samples.append(
                {"cpu_ticks": cpu_ticks(pid), "phase": state["phase"], "stack": stack}
            )
            if len(samples) >= 10000:
                raise RuntimeError("sample cap reached; no incomplete profile accepted")
    finally:
        done.set()
        worker.join(timeout=0.1)
        os.close(pidfd)
        entry_breakpoint.delete()
        gdb.events.stop.disconnect(stopped)
        gdb.events.exited.disconnect(exited)
        output.write_text(
            json.dumps(
                {
                    "scope": "Intrusive single-thread CPU-tick-triggered GDB stacks from main to exit; original native code unchanged. Approximate hotspot evidence only, not perf, total instructions, exact CPU attribution, isolated timings or device acceptance. No source IDs inferred from unsymbolized/inlined frames.",
                    "clock_hz": clock_hz,
                    "libraries_at_main": libraries,
                    "wall_seconds_instrumented": time.monotonic() - started,
                    "native_exit": state["exit"],
                    "bake_entries": entries,
                    "worker_error": state["worker_error"],
                    "samples": samples,
                },
                indent=2,
            )
            + "\n"
        )


sample(Path(os.environ["OUTSHINE_CAPTURE_PROFILE_JSON"]))
