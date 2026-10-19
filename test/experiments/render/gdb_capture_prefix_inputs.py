"""Read actual capture trees and bounded first-callback controls, without a build.

GDB-only, at main of the SHA-pinned #801 x86-64/libc++ exporter. The ABI is
deliberately frozen; unknown binaries are rejected, never guessed. Only debugger
breakpoints and inferior memory/register reads are used. Output stays private.
"""

import hashlib
import json
import os
import struct
from pathlib import Path

import gdb

FROZEN = "012d9aa8c525e98f46466d99ba30f8c5273dd4040fb012b92f5733e2979a7d8d"
BUILD = "outshine::PlanHierarchy::Build(std::__1::span<outshine::BoxOf<double> const, 18446744073709551615ul>)"
TRACE = "outshine::Generators::BuildingSurface::Trace(outshine::Generators::BuildingSurface::Ray const&, double, double, std::__1::vector<double, std::__1::allocator<double> >&) const"


class TraceEntry(gdb.Breakpoint):
    def __init__(self, records):
        super().__init__(TRACE, internal=True)
        self.records = records
        self.finishes = []
        self.enabled = False

    def stop(self):
        frame, caller = gdb.newest_frame(), gdb.newest_frame().older()
        if caller is None or "::CaptureFaces(" not in (caller.name() or ""):
            raise RuntimeError("unknown Source::Trace caller; no ABI inference")
        inferior = gdb.selected_inferior()
        ray = bytes(inferior.read_memory(int(frame.read_register("rdx")), 48))
        record = self.records[-1]
        if ray.hex() not in record["first_callbacks"]:
            witness = {
                "header_hex": bytes(
                    inferior.read_memory(int(frame.read_register("rsi")), 144)
                ).hex(),
                "minimum_hex": frame.read_register("xmm0").bytes[:8].hex(),
                "maximum_hex": frame.read_register("xmm1").bytes[:8].hex(),
            }
            record["first_callbacks"][ray.hex()] = witness
            result = int(frame.read_register("rdi"))

            class Finish(gdb.FinishBreakpoint):
                def stop(self):
                    flag = bytes(inferior.read_memory(result + 56, 1))[0]
                    if flag not in (0, 1):
                        raise RuntimeError("unknown native optional Hit storage")
                    witness["has_hit"] = bool(flag)
                    if flag:
                        witness["hit_hex"] = bytes(
                            inferior.read_memory(result, 49)
                        ).hex()
                    return False

            self.finishes.append(Finish(frame, internal=True))
        if len(record["first_callbacks"]) == 4:
            self.enabled = False
        return False


class BuildEntry(gdb.Breakpoint):
    def __init__(self, records, trace):
        super().__init__(BUILD, internal=True)
        self.records, self.trace, self.finishes = records, trace, []

    def stop(self):
        frame = gdb.newest_frame()
        caller = frame.older()
        if caller is None or not (caller.name() or "").startswith(
            "outshine::Generators::CaptureBuildingSurfaces("
        ):
            return False
        result, pointer, count = (
            int(frame.read_register(name)) for name in ("rdi", "rsi", "rdx")
        )
        if not 0 < count <= 65536:
            raise RuntimeError("unexpected source span; no unchecked memory read")
        inferior = gdb.selected_inferior()
        bounds = bytes(inferior.read_memory(pointer, count * 48))
        records, trace = self.records, self.trace

        class Finish(gdb.FinishBreakpoint):
            def stop(self):
                raw = bytes(inferior.read_memory(result, 56))
                a, b, c, d, e, f, valid = struct.unpack("<6QB7x", raw)
                if (
                    valid != 1
                    or b - a != (2 * count - 1) * 64
                    or e - d != count * 4
                    or not a <= b <= c
                    or not d <= e <= f
                ):
                    raise RuntimeError("unverified expected/vector storage")
                records.append(
                    {
                        "count": count,
                        "bounds_hex": bounds.hex(),
                        "nodes_hex": bytes(inferior.read_memory(a, b - a)).hex(),
                        "order_hex": bytes(inferior.read_memory(d, e - d)).hex(),
                        "first_callbacks": {},
                    }
                )
                trace.enabled = True
                return False

        self.finishes.append(Finish(frame, internal=True))
        return False


def run():
    binary = Path(gdb.current_progspace().filename)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != FROZEN:
        raise RuntimeError("unverified frozen x86-64/libc++ ABI")
    records, status = [], {"exit": None}

    def exited(event):
        status["exit"] = getattr(event, "exit_code", -1)

    gdb.events.exited.connect(exited)
    trace = TraceEntry(records)
    build = BuildEntry(records, trace)
    gdb.execute("continue", to_string=True)
    Path(os.environ["OUTSHINE_CAPTURE_PREFIX_JSON"]).write_text(
        json.dumps(
            {
                "native_exit": status["exit"],
                "binary_sha256": FROZEN,
                "reader_sha256": hashlib.sha256(
                    Path(__file__).read_bytes()
                ).hexdigest(),
                "trees": records,
            }
        )
        + "\n"
    )
    build.delete()
    trace.delete()
    if status["exit"] != 0:
        raise RuntimeError("native exporter did not terminate successfully")


run()
