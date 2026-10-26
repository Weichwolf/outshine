"""Explicit window demand before detail expansion; the coarse exterior stays complete."""

from dataclasses import dataclass, replace
from functools import lru_cache
from types import MappingProxyType

import numpy as np
import opening_details
from outshine_ref.buildings.modelling.prism_union import frozen
from outshine_ref.buildings.rooflight_panels import triangle_surface
from terraced_facade_details import details
from terraced_facade_prepare import positions
from terraced_facade_surface import compact, joined

MAX_WINDOWS = 8192


@dataclass(frozen=True)
class MeshSpan:
    mesh: object
    first_face: int
    face_count: int
    identifier: str


@dataclass(frozen=True)
class WindowPatch:
    identifier: str
    mesh: object


@dataclass(frozen=True)
class WindowDraw:
    source_id: str
    spans: tuple
    detailed_ids: tuple
    generated_ids: tuple
    released_ids: tuple
    fallback_ids: tuple
    generated_buffer_bytes: int
    resident_detail_bytes: int


def buffer_bytes(mesh):
    return sum(value.nbytes for value in mesh.values())


def window_closed(prepared, window, *, template_builder=None):
    """Reuse the existing closed recipe without floors, stair building or CSG."""
    facade = replace(prepared.facade, windows=(window,))
    raw = details(
        prepared.local, facade, legacy_trim=False, template_builder=template_builder
    )
    raw = raw | {"vertices": positions(raw["vertices"], prepared.mass.core)}
    return raw


def window_patch(prepared, window, *, template_builder=None):
    raw = window_closed(prepared, window, template_builder=template_builder)
    mesh = triangle_surface(raw, 10)
    if len(mesh["faces"]) != len(raw["faces"]):
        raise ValueError("Window draw cannot drop a physical triangle")
    mesh["materials"] = raw["materials"]
    mesh = MappingProxyType({key: frozen(value) for key, value in mesh.items()})
    return WindowPatch(window.identifier, mesh)


def coarse_spans(coarse):
    owners = coarse.triangle_owners
    if not len(owners):
        return ()
    starts = np.r_[0, np.flatnonzero(owners[1:] != owners[:-1]) + 1]
    ends = np.r_[starts[1:], len(owners)]
    return tuple(
        MeshSpan(coarse.mesh, int(a), int(b - a), coarse.owner_ids[owners[a]])
        for a, b in zip(starts, ends, strict=True)
    )


def repeated_dimensions(windows, resident, identifiers):
    seen = set()
    for identifier in identifiers:
        if identifier in resident:
            continue
        window = windows[identifier]
        key = window.width_m, window.height_m
        if key in seen:
            return True
        seen.add(key)
    return False


class WindowDetails:
    """Caller-owned residency, bounded in window count; no visibility or DB policy."""

    def __init__(self, prepared, *, portals=(), max_windows=128):
        if type(max_windows) is not int or not 0 <= max_windows <= MAX_WINDOWS:
            raise ValueError("Window residency requires an integer bounded capacity")
        windows = prepared.facade.windows
        if len(windows) > MAX_WINDOWS:
            raise ValueError("Window plan exceeds the reference capacity")
        self.windows = MappingProxyType(
            {window.identifier: window for window in windows}
        )
        if len(self.windows) != len(windows):
            raise ValueError("Window plan contains duplicate identities")
        self.prepared = prepared
        self.max_windows = max_windows
        self.coarse = compact(prepared, portals=portals)
        if tuple(self.windows) != self.coarse.window_ids:
            raise ValueError("Coarse and detailed window identities disagree")
        self.base_spans = coarse_spans(self.coarse)
        self.resident = MappingProxyType({})

    def request(self, identifiers):
        if not isinstance(identifiers, (tuple, list, set, frozenset)):
            raise TypeError("Explicit window IDs require a bounded collection")
        if len(identifiers) > len(self.windows) or any(
            not isinstance(identifier, str) for identifier in identifiers
        ):
            raise ValueError("Invalid window request")
        selected = frozenset(identifiers)
        if len(selected) != len(identifiers) or not selected <= self.windows.keys():
            raise ValueError("Window request has duplicate or foreign identities")
        ordered = tuple(
            identifier for identifier in self.windows if identifier in selected
        )
        return ordered[: self.max_windows], ordered[self.max_windows :]

    def update(self, identifiers):
        admitted, fallback = self.request(identifiers)
        # Commit only after every new product and the complete draw list succeed.
        resident, generated = {}, []
        templates = None
        reuse = repeated_dimensions(self.windows, self.resident, admitted)
        try:
            for identifier in admitted:
                patch = self.resident.get(identifier)
                if patch is None:
                    if reuse and templates is None:
                        templates = lru_cache(maxsize=16)(opening_details.template)
                    patch = window_patch(
                        self.prepared,
                        self.windows[identifier],
                        template_builder=templates,
                    )
                    generated.append(identifier)
                resident[identifier] = patch
        finally:
            if templates is not None:
                templates.cache_clear()
        released = tuple(
            identifier for identifier in self.resident if identifier not in resident
        )
        spans = tuple(
            span for span in self.base_spans if span.identifier not in resident
        )
        spans += tuple(
            MeshSpan(patch.mesh, 0, len(patch.mesh["faces"]), identifier)
            for identifier, patch in resident.items()
        )
        generated_bytes = sum(
            buffer_bytes(resident[identifier].mesh) for identifier in generated
        )
        resident_bytes = sum(buffer_bytes(patch.mesh) for patch in resident.values())
        result = WindowDraw(
            self.prepared.mass.identifier,
            spans,
            admitted,
            tuple(generated),
            released,
            fallback,
            generated_bytes,
            resident_bytes,
        )
        self.resident = MappingProxyType(resident)
        return result


def materialize(draw):
    """Diagnostic export only; residency/draw references themselves copy no meshes."""
    products = []
    for span in draw.spans:
        mesh = span.mesh
        face_slice = slice(span.first_face, span.first_face + span.face_count)
        vertices, inverse = np.unique(mesh["faces"][face_slice], return_inverse=True)
        products.append(
            {
                key: inverse.reshape(-1, 3).astype(np.uint32)
                if key == "faces"
                else value[face_slice]
                if key == "materials"
                else value[vertices]
                for key, value in mesh.items()
            }
        )
    return joined(tuple(products))
