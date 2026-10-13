"""Compatibility imports; implementation lives in the installed reference library."""

import numpy as np
from outshine_ref.vegetation.placement_patterns import (
    EMPTY_SITES,
    MAX_SITES,
    DistancePattern,
)

__all__ = ["EMPTY_SITES", "MAX_SITES", "DistancePattern", "np"]
