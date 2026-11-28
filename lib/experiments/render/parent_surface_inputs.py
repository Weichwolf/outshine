"""Read one supplied prepared parent, retaining anonymous source ordinals and cells."""

import hashlib
import json
import tarfile
from contextlib import contextmanager
from pathlib import Path

import numpy as np
from building_plan_inputs import read_plans
from prepared_surface_recipes import decode


@contextmanager
def open_parent(path):
    with tarfile.open(path) as archive:
        yield ParentInputs(path, archive)


class ParentInputs:
    def __init__(self, path, archive):
        self.path = Path(path)
        entry = json.loads((self.path.parent / "manifest.json").read_text())[
            self.path.name
        ]
        raw = self.path.read_bytes()
        if (
            len(raw) != entry["bytes"]
            or hashlib.sha256(raw).hexdigest() != entry["sha256"]
        ):
            raise ValueError("original parent archive manifest mismatch")
        self.archive_sha256 = entry["sha256"]
        self.tar = archive
        members = [m for m in self.tar.getmembers() if m.name.endswith("/input.json")]
        if len(members) != 1 or not members[0].isfile() or members[0].size > 1048576:
            raise ValueError("one bounded parent metadata member required")
        self.metadata = json.load(self.tar.extractfile(members[0]))
        self.parent = self.metadata["parent"]
        if members[0].name != self.parent + "/input.json":
            raise ValueError("parent metadata identity mismatch")
        self.packages = {p["file"]: p for p in self.metadata["native_packages"]}
        if len(self.packages) != len(self.metadata["native_packages"]):
            raise ValueError("duplicate parent package identity")
        self.forms = {f["cell"]: f["file"] for f in self.metadata["forms"]}
        if (
            len(self.forms) != len(self.metadata["forms"])
            or any(type(c) is not int or not 1 <= c <= 64 for c in self.forms)
            or len(set(self.forms.values())) != len(self.forms)
        ):
            raise ValueError("unique bounded native form cells required")
        self.plans = read_plans(self.member(self.metadata["index"]))
        self.cells = np.array([r["cell"] for r in self.plans["records"]], np.uint8)
        if not set(self.cells).issubset(self.forms):
            raise ValueError("source cell has no original form block")
        self.form_reads = self.form_bytes = 0

    def member(self, name):
        record = self.packages[name]
        member = self.tar.getmember(self.parent + "/" + name)
        if not member.isfile() or not 0 < member.size <= 64 * 1024**2:
            raise ValueError("bounded regular native member required")
        raw = self.tar.extractfile(member).read()
        if (
            len(raw) != record["native_bytes"]
            or hashlib.sha256(raw).hexdigest() != record["sha256"]
        ):
            raise ValueError("original native package digest mismatch")
        return raw

    def iter_forms(self, cells=None):
        selected = set(self.forms) if cells is None else set(cells)
        if not selected.issubset(self.forms):
            raise ValueError("requested cell has no native form block")
        # Physical order avoids repeatedly rewinding gzip for logical cell order.
        ordered = sorted(
            selected,
            key=lambda c: (
                self.tar.getmember(self.parent + "/" + self.forms[c]).offset_data
            ),
        )
        for cell in ordered:
            raw = self.member(self.forms[cell])
            surfaces, _ = decode(raw)
            ordinals = np.flatnonzero(self.cells == cell)
            if len(surfaces) != len(ordinals):
                raise ValueError("form/source-cell membership count mismatch")
            for ordinal, surface in zip(ordinals, surfaces, strict=True):
                self.check_surface(int(ordinal), surface)
            self.form_reads += 1
            self.form_bytes += len(raw)
            yield cell, ordinals, surfaces

    def check_surface(self, ordinal, surface):
        summary = self.plans["summaries"][ordinal]
        if (surface is not None) != summary["supported"]:
            raise ValueError("source/form support association differs")
        if surface is None:
            return
        frame = np.array([surface[k] for k in ("origin", "east", "north", "up")])
        if (
            not np.array_equal(frame, summary["frame"])
            or not np.array_equal(surface["bounds"], summary["bounds"])
            or surface["minimum_height_m"] != summary["minimum_height_m"]
            or surface["wall_colour"]
            != (list(summary["colour"]) if summary["colour"] is not None else None)
            or surface["face_offsets"][-1] != summary["faces"]
        ):
            raise ValueError(
                "source/form frame, bounds, colour or face association differs"
            )
