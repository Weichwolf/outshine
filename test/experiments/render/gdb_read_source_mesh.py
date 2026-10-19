"""Read one-source Raised streams from a SHA-pinned unchanged x86-64/libc++ probe.

GDB-only at main. Inferior memory/register reads and breakpoints only; no calls,
input writes or patched execution. Native output validates all six vector layouts.
"""

import hashlib
import json
import os
import re
import struct
import tempfile
from pathlib import Path

import gdb

FROZEN = "60679166753fd2cc6856524329a1eba13f59b61fa6cfdb13b63d5f5a0a6d9a33"
MESH = "_ZNK8outshine10Generators12BuildingMesh4MeshERKNS_13StructurePlanERNS_11MeshScratchERNS_6RaisedE"
STREAMS = (
    ("walls", 24),
    ("roofs", 24),
    ("wall_indices", 4),
    ("roof_indices", 4),
    ("colours", 4),
    ("metres", 4),
)


def read_streams(inferior, address):
    words = struct.unpack("<18Q", bytes(inferior.read_memory(address, 144)))
    result = {}
    for index, (name, stride) in enumerate(STREAMS):
        begin, end, capacity = words[3 * index : 3 * index + 3]
        if (
            not begin <= end <= capacity
            or (not begin and (end or capacity))
            or end - begin > 16 * 1024**2
            or capacity - begin > 64 * 1024**2
            or (end - begin) % stride
        ):
            raise RuntimeError("unverified frozen vector layout: " + name)
        result[name] = (
            bytes(inferior.read_memory(begin, end - begin)) if end > begin else b""
        )
    return result


def main():
    binary = Path(gdb.current_progspace().filename)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != FROZEN:
        raise RuntimeError("unsupported binary; do not infer another ABI")
    if gdb.newest_frame().architecture().name() != "i386:x86-64":
        raise RuntimeError("frozen x86-64 ABI required")
    output = Path(os.environ["OUTSHINE_SOURCE_MESH_OUTPUT"]).resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise RuntimeError("fresh system-temp output required")
    output.mkdir(parents=True, exist_ok=False)
    captures = []
    finishes = []
    state = {"exit": None}
    inferior = gdb.selected_inferior()
    expected_detail = int(os.environ.get("OUTSHINE_SOURCE_EXPECTED_DETAIL", "0"))
    if expected_detail not in (0, 1):
        raise RuntimeError("explicit Fine/Shell control required")
    opening_flag = os.environ.get("OUTSHINE_SOURCE_EXPECTED_OPENINGS", "1")
    if opening_flag not in ("0", "1"):
        raise RuntimeError("explicit openings control required")
    expected_openings = opening_flag == "1"

    class Entry(gdb.Breakpoint):
        def stop(self):
            # System V argument 4 is Raised&; verified at frozen symbol entry.
            address = int(gdb.newest_frame().read_register("rcx"))
            if any(read_streams(inferior, address).values()):
                raise RuntimeError("one-source empty append target required")
            if len(captures) >= 2:
                raise RuntimeError("unexpected additional mesh call")
            index = len(captures)
            captures.append(None)

            class Finish(gdb.FinishBreakpoint):
                def stop(self):
                    streams = read_streams(inferior, address)
                    record = {}
                    directory = output / str(index)
                    directory.mkdir()
                    for name, stride in STREAMS:
                        payload = streams[name]
                        (directory / (name + ".bin")).write_bytes(payload)
                        record[name] = {
                            "bytes": len(payload),
                            "count": len(payload) // stride,
                            "sha256": hashlib.sha256(payload).hexdigest(),
                        }
                    captures[index] = record
                    return False

                def out_of_scope(self):
                    state["unexpected_unwind"] = True

            finishes.append(Finish(gdb.newest_frame(), internal=True))
            return False

    description = gdb.execute("info address " + MESH, to_string=True)
    address = re.search(r" at (0x[0-9a-fA-F]+)", description)
    if address is None:
        raise RuntimeError("frozen mesh symbol not found")
    entry = Entry("*" + address[1], internal=True)

    def exited(event):
        state["exit"] = getattr(event, "exit_code", -1)

    gdb.events.exited.connect(exited)
    gdb.execute("continue")
    if (
        state["exit"] != 0
        or state.get("unexpected_unwind")
        or len(captures) != 2
        or None in captures
        or captures[0] != captures[1]
    ):
        raise RuntimeError("native one-source captures did not terminate identically")
    native = json.loads(Path(os.environ["OUTSHINE_SOURCE_NATIVE_JSON"]).read_text())
    if len(native["sources"]) != 1 or len(native["results"]) != 2:
        raise RuntimeError("not one source")
    for row, capture in zip(native["results"], captures, strict=True):
        if (
            not row["accepted"]
            or row["projection_calls"]
            or row["mesh_calls"]
            != [
                {
                    "accepted": True,
                    "detail": expected_detail,
                    "error": None,
                    "openings": expected_openings,
                    "source": 0,
                }
            ]
        ):
            raise RuntimeError("not a successful original selected mesh")
        if (
            capture["walls"]["count"] != row["wall_vertices"]
            or capture["roofs"]["count"] != row["roof_vertices"]
            or sum(s["bytes"] for s in capture.values()) != row["built_bytes"]
            or (capture["wall_indices"]["count"] + capture["roof_indices"]["count"])
            // 3
            != row["fallback_triangles"]
        ):
            raise RuntimeError("native JSON does not validate vector boundaries")
    (output / "receipt.json").write_text(
        json.dumps(
            {
                "binary_sha256": FROZEN,
                "state": state,
                "detail": expected_detail,
                "openings": expected_openings,
                "captures": captures,
                "scope": "Single-source original selected streams; all six vectors match original native counts and independent whole/cell invocations; no performance or world/device gate.",
            },
            indent=2,
        )
        + "\n"
    )
    entry.enabled = False


main()
