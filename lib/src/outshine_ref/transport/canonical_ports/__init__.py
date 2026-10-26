"""Bounded source/DEM-derived canonical transport ports, independent of core order.

The current executable vertical slice qualifies unbranched admitted ground
components. Other domains remain Pending; this is not a whole-world/native-plan
admission. Native IDs, original source references, omissions and height mappings
remain explicit. No cache, job manager or native product codec is introduced.
"""

from ._api import derive_canonical_ports
from ._types import (
    PortBudget,
    PortDemand,
    PortNode,
    PortResult,
    PortSource,
    PortSpan,
    PortValue,
)

__all__ = [
    "PortBudget",
    "PortDemand",
    "PortNode",
    "PortResult",
    "PortSource",
    "PortSpan",
    "PortValue",
    "derive_canonical_ports",
]
