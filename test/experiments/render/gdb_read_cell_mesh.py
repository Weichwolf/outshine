"""Read append-only whole-cell outputs from the unchanged SHA-pinned probe.

Each native call retains separate six-stream ranges. Massed calls have no source
ID: only the complete cell and the set of lumped sources are known, not a guessed
per-block membership. No inferior calls, writes, branch patches or native builds.
"""

import hashlib
import json
import os
import re
import sys
import tempfile
from pathlib import Path

import gdb

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gdb_read_source_mesh import FROZEN, MESH, STREAMS, read_streams
from native_cell_mesh_validation import validate_row

PROJECTED = "_ZN8outshine10Generators22StructurePlanSelection13EmitProjectedERKNS1_7CommandERKNS0_7RawTileERKNS_15StructureMesherERNS_11MeshScratchERNS0_9BakedTileE"


def without_timing(value):
    if isinstance(value, dict):
        return {k: without_timing(v) for k, v in value.items() if k != "elapsed_s"}
    if isinstance(value, list):
        return [without_timing(v) for v in value]
    return value


def main():
    binary = Path(gdb.current_progspace().filename)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != FROZEN:
        raise RuntimeError("unsupported binary")
    if gdb.newest_frame().architecture().name() != "i386:x86-64":
        raise RuntimeError("frozen x86-64 ABI required")
    out = Path(os.environ["OUTSHINE_SOURCE_MESH_OUTPUT"]).resolve()
    native_path = Path(os.environ["OUTSHINE_SOURCE_NATIVE_JSON"])
    independent = Path(os.environ["OUTSHINE_SOURCE_ORIGINAL_JSON"])
    if native_path.resolve() == independent.resolve():
        raise RuntimeError("independent native output required")
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise RuntimeError("fresh system-temp output required")
    original = json.loads(independent.read_text())
    manifest = json.loads((independent.parent / "input.json").read_text())
    sources = manifest["original_sources"]
    if not 1 <= len(sources) <= 32 or original["parent"] != manifest["parent"]:
        raise RuntimeError("bounded complete original cell required")
    out.mkdir(parents=True, exist_ok=False)
    batches, finishes, state = [], [], {"exit": None}
    inferior = gdb.selected_inferior()

    class Entry(gdb.Breakpoint):
        def __init__(self, address, register, offset, emitter):
            super().__init__("*" + address, internal=True)
            self.register, self.offset, self.emitter = register, offset, emitter

        def stop(self):
            address = int(gdb.newest_frame().read_register(self.register)) + self.offset
            emitter = self.emitter
            before = read_streams(inferior, address)
            if not any(before.values()):
                if len(batches) >= 2:
                    raise RuntimeError("unexpected additional bake")
                batches.append({"address": address, "streams": before, "calls": []})
            batch = batches[-1]
            if batch["address"] != address or batch["streams"] != before:
                raise RuntimeError("append target changed between mesh calls")
            if len(batch["calls"]) >= len(sources):
                raise RuntimeError("unbounded mesh calls")

            class Finish(gdb.FinishBreakpoint):
                def stop(self):
                    after = read_streams(inferior, address)
                    ranges = {"emitter": emitter}
                    for name, stride in STREAMS:
                        if not after[name].startswith(before[name]):
                            raise RuntimeError("mesh modified previous source bytes")
                        ranges[name] = [
                            len(before[name]) // stride,
                            len(after[name]) // stride,
                        ]
                    batch["calls"].append(ranges)
                    batch["streams"] = after
                    return False

                def out_of_scope(self):
                    state["unexpected_unwind"] = True

            finishes.append(Finish(gdb.newest_frame(), internal=True))
            return False

    entries = []
    # Frozen SysV ABI: Mesh argument 4 is Raised&, while EmitProjected argument
    # 6 is BakedTile&. The latter's first 16 bytes are libc++ shared_ptr, then
    # Raised. Symbol disassembly reads roof vector at BakedTile+0x28, confirming
    # Raised+24 after this 16-byte prefix. Final independent vector checks follow.
    for name, register, offset, emitter in (
        (MESH, "rcx", 0, "Mesh"),
        (PROJECTED, "r9", 16, "EmitProjected"),
    ):
        description = gdb.execute("info address " + name, to_string=True)
        symbol = re.search(r" at (0x[0-9a-fA-F]+)", description)
        if symbol is None:
            raise RuntimeError("frozen emitter symbol missing")
        entries.append(Entry(symbol[1], register, offset, emitter))

    def exited(event):
        state["exit"] = getattr(event, "exit_code", -1)

    gdb.events.exited.connect(exited)
    gdb.execute("continue")
    native = json.loads(native_path.read_text())
    if state["exit"] != 0 or state.get("unexpected_unwind") or len(batches) != 2:
        raise RuntimeError("cell observations did not terminate")
    if without_timing(native) != without_timing(original):
        raise RuntimeError("instrumented native observations differ")
    if len(native["sources"]) != len(sources) or len(native["results"]) != 2:
        raise RuntimeError("source or invocation count mismatch")
    for i, (source, saved) in enumerate(zip(native["sources"], sources, strict=True)):
        if (
            source["index"] != i
            or source["cell"] != manifest["original_cell"]
            or [source["kind"], source["id"]] != saved["original_source_id"]
        ):
            raise RuntimeError("original source mapping changed")
    for i, (batch, row) in enumerate(zip(batches, native["results"], strict=True)):
        target = out / str(i)
        target.mkdir()
        batch["capture"] = {}
        for name, stride in STREAMS:
            payload = batch["streams"][name]
            (target / (name + ".bin")).write_bytes(payload)
            batch["capture"][name] = {
                "bytes": len(payload),
                "count": len(payload) // stride,
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        validate_row(row, batch, len(sources))
    if (
        batches[0]["capture"] != batches[1]["capture"]
        or batches[0]["calls"] != batches[1]["calls"]
    ):
        raise RuntimeError("whole/cell streams or call ranges differ")
    (out / "receipt.json").write_text(
        json.dumps(
            {
                "binary_sha256": FROZEN,
                "state": state,
                "original_sources": sources,
                "captures": [b["capture"] for b in batches],
                "call_ranges": [b["calls"] for b in batches],
                "lumped_local_sources": [
                    i for i, d in enumerate(native["results"][0]["details"]) if d == 2
                ],
                "projected_local_sources": [
                    m["source"]
                    for c in native["results"][0]["projection_calls"]
                    for m in c["members"]
                ],
                "scope": "Complete original cell; actual append ranges. Normal triangle ranges bind source; optional colour/metre backfill is not semantic per-source ownership. Mass and projected calls retain only collective membership, not per-member face/part mapping. Whole/cell and independent native outputs match. Frozen EmitProjected ABI r9+16; no runtime/device admission.",
            },
            indent=2,
        )
        + "\n"
    )
    for entry in entries:
        entry.enabled = False


main()
