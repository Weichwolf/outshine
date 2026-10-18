"""Isolate the replacement predicate; retain constructor/XYZ/schema/mask checks."""

import contextlib
from unittest.mock import patch

import numpy as np
from outshine_ref.geometry.surface_patches import _field
from stair_composition_boundary import equal_coverage

LIBRARY_COVERAGE = _field._same_coverage


@contextlib.contextmanager
def composition(method, records, *, captured=None):
    if method not in ("general", "coverage", "oriented"):
        raise ValueError("explicit general/coverage/oriented comparison required")

    def compare(before, after):
        result = (
            LIBRARY_COVERAGE(before, after)
            if method == "coverage"
            else equal_coverage(before, after, method)
        )
        records.append(
            {"before": len(before), "after": len(after), "equal": bool(result)}
        )
        if captured is not None:
            captured.append((np.asarray(before, object), np.asarray(after, object)))
        return result

    with patch.object(_field, "_same_coverage", compare):
        yield
