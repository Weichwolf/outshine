"""Same-content eager control; prebuild every original source window before demand."""

import hashlib
from dataclasses import replace

from facade_programme_plan import plan_programme
from facade_programme_products import programme_products
from programme_window_demand import (
    ProgrammeWindowDetails,
    ProgrammeWindowDraw,
    mesh_span,
)
from terraced_crown_plan import plan as roof_plan
from terraced_mass_sources import sources
from terraced_terrace_plan import plan as terrace_plan
from window_detail_demand import MAX_WINDOWS, WindowDetails


def source_product(index=0, kind="cascade", programme="ribbon-screens"):
    crown = roof_plan(sources()[index], kind)
    terrace = terrace_plan(crown.body)
    planned = plan_programme(terrace.prepared, programme, portals=terrace.portals)
    return programme_products(crown, planned)


def fingerprint(mesh):
    value = hashlib.sha256()
    for key, array in sorted(mesh.items()):
        value.update(key.encode())
        value.update(str((array.dtype, array.shape)).encode())
        value.update(array.tobytes())
    return value.hexdigest()


class EagerProgrammeWindows:
    """Diagnostic control only; keep all prebuilt windows for the same capped demand."""

    def __init__(self, product, *, max_windows=8, selected_ids=()):
        self.product = product
        self.full = ProgrammeWindowDetails(product, max_windows=MAX_WINDOWS)
        self.identifiers = tuple(self.full.windows.windows)
        self.complete = self.full.update(self.identifiers, selected_ids=selected_ids)
        facade = product.body.facade.source
        self.selector = WindowDetails(
            facade.prepared,
            portals=facade.portals,
            max_windows=max_windows,
            facade_product=facade,
        )
        source_id = product.plan.source_id
        self.fixed = tuple(
            span
            for span in self.complete.spans
            if span.identifier in (source_id + "/programme-layers", source_id + "/roof")
        )

    def update(self, identifiers):
        admitted, fallback = self.selector.request(identifiers)
        selected = frozenset(admitted)
        coarse = tuple(
            span
            for span in self.full.windows.base_spans
            if span.identifier not in selected
        )
        raw = coarse + tuple(
            mesh_span(self.full.windows.resident[i].mesh, i) for i in admitted
        )
        windows = replace(
            self.complete.windows,
            spans=raw,
            detailed_ids=admitted,
            generated_ids=(),
            released_ids=(),
            fallback_ids=fallback,
            generated_buffer_bytes=0,
        )
        spans = tuple(
            part
            for span in coarse
            for part in self.full.coarse_spans[span.first_face, span.face_count]
        )
        spans += tuple(part for i in admitted for part in self.full.detail_spans[i])
        spans += self.full.body_spans + self.fixed
        return ProgrammeWindowDraw(
            self.product.plan.source_id,
            spans,
            windows,
            0,
            self.complete.resident_layer_bytes,
        )
