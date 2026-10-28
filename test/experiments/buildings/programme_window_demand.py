"""Explicit window demand and borrowed programme/roof spans, without draw copies."""

from copy import copy
from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
from bay_facade_mesh import expand
from facade_programme_products import ProgrammeProducts, programme_selection
from outshine_ref.buildings.modelling.prism_union import frozen
from window_detail_demand import MeshSpan, WindowDetails, buffer_bytes


@dataclass(frozen=True)
class ProgrammeWindowDraw:
    source_id: str
    spans: tuple
    windows: object
    generated_layer_bytes: int
    resident_layer_bytes: int


def cap_mask(mesh, height):
    xyz = mesh["vertices"][mesh["faces"]]
    cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    return (np.max(abs(xyz[:, :, 2] - height), axis=1) <= 1e-9) & (cross[:, 2] > 0)


def retained_spans(span, height, *, cap=None):
    """Same highest-cap predicate as the dense reference; retain index ranges."""
    if height is None or not span.face_count:
        return (span,), 0
    if cap is None:
        cap = cap_mask(span.mesh, height)[
            span.first_face : span.first_face + span.face_count
        ]
    if not cap.any():
        return (span,), 0
    starts = np.flatnonzero(np.r_[True, cap[1:] != cap[:-1]])
    ends = np.r_[starts[1:], len(cap)]
    return tuple(
        MeshSpan(span.mesh, span.first_face + int(a), int(b - a), span.identifier)
        for a, b in zip(starts, ends, strict=True)
        if not cap[a]
    ), int(cap.sum())


def mesh_span(mesh, identifier):
    return MeshSpan(mesh, 0, len(mesh["faces"]), identifier)


class ProgrammeWindowDetails:
    """Bounded window residency and one current layer product; physical work separate.

    Requests are explicit source IDs, without visibility/LOD admission. Output
    spans borrow existing geometry; materialize only for diagnosis. The bound
    source product and its geometry remain unchanged while these spans live.
    Failed compositions retain previous residency/layers. Ownership/plan changes
    require a new instance rather than reusing another source's prepared skin.
    """

    def __init__(self, product, *, max_windows=128):
        if not isinstance(product, ProgrammeProducts):
            raise TypeError("Existing source-bound programme products required")
        self.product, self.plan = product, product.plan
        self.source, self.source_plan = product.source, product.source.plan
        self.facade = product.body.facade.source
        self.windows = WindowDetails(
            self.facade.prepared,
            portals=self.facade.portals,
            max_windows=max_windows,
            facade_product=self.facade,
        )
        self.height = (
            product.source.plan.body.top_m if product.source.plan.crown else None
        )
        self.coarse_spans, self.body_spans, caps = {}, [], 0
        mask = (
            cap_mask(self.windows.coarse.mesh, self.height)
            if self.height is not None
            else None
        )
        for span in self.windows.base_spans:
            local = (
                None
                if mask is None
                else mask[span.first_face : span.first_face + span.face_count]
            )
            kept, count = retained_spans(span, self.height, cap=local)
            self.coarse_spans[span.first_face, span.face_count] = kept
            caps += count
        for index, mesh in enumerate(
            (*product.body.additions, product.body.portal_frames)
        ):
            span = mesh_span(mesh, self.plan.source_id + "/body-part/" + str(index))
            kept, count = retained_spans(span, self.height)
            self.body_spans.extend(kept)
            caps += count
        if self.height is not None and not caps:
            raise ValueError(
                "Expected highest body cap is missing from the composed draw"
            )
        self.body_spans = tuple(self.body_spans)
        self.detail_spans = {}
        self.layer_ids, self.layers = None, None

    def _layers(self, selected):
        if self.layers is not None and selected == self.layer_ids:
            return self.layers, 0
        mesh, _owners = expand(self.plan, selected, mode="outer")
        mesh = MappingProxyType({key: frozen(value) for key, value in mesh.items()})
        return mesh, buffer_bytes(mesh)

    def _windows(self, staged, windows):
        details = {}
        spans = []
        for span in windows.spans:
            if span.mesh is staged.coarse.mesh:
                spans.extend(self.coarse_spans[span.first_face, span.face_count])
            else:
                retained = self.detail_spans.get(span.identifier)
                previous = self.windows.resident.get(span.identifier)
                if retained is None or previous is not staged.resident[span.identifier]:
                    retained, _count = retained_spans(span, self.height)
                details[span.identifier] = retained
                spans.extend(retained)
        return tuple(spans), details

    def update(self, identifiers, *, selected_ids=None, roof_near=False):
        if type(roof_near) is not bool:
            raise ValueError("Explicit roof detail selection required")
        if (
            self.product.plan is not self.plan
            or self.product.body.facade.source is not self.facade
            or self.product.source is not self.source
            or self.source.plan is not self.source_plan
        ):
            raise ValueError(
                "Programme source changed after binding the detail product"
            )
        selected = programme_selection(self.plan, selected_ids)
        staged = copy(self.windows)
        windows = staged.update(identifiers)
        spans, details = self._windows(staged, windows)
        layers, generated = self._layers(selected)
        spans += self.body_spans + (
            mesh_span(layers, self.plan.source_id + "/programme-layers"),
        )
        if self.product.source.plan.crown is not None:
            if roof_near:
                self.product.source.physical_roof()
            roof = (
                self.product.source.roof_skin
                if roof_near
                else self.product.source.coarse_roof
            )
            spans += (mesh_span(roof, self.plan.source_id + "/roof"),)
        result = ProgrammeWindowDraw(
            self.plan.source_id, spans, windows, generated, buffer_bytes(layers)
        )
        self.windows, self.detail_spans = staged, details
        self.layer_ids, self.layers = selected, layers
        return result
