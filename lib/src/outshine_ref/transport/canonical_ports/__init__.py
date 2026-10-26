"""Bounded source/DEM-derived canonical transport ports, independent of core order.

Unbranched admitted ground components and explicitly enabled straight bridge
height recipes are executable. Other domains remain Pending; this is not a
whole-world/native-plan or bridge-clearance admission. Native IDs, original
source references, omissions and height mappings remain explicit. No cache,
job manager or native product codec is introduced.
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
