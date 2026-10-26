"""Bound normalized source identities, numeric demands and explicit local results."""

from collections.abc import Mapping
from dataclasses import dataclass, field
from math import isfinite

from ...contracts import immutable
from ..port_heights import PortHeightBudget


def node_id(value):
    if type(value) is not int or not 0 < value < 2**64:
        raise ValueError("an existing normalized uint64 source node is required")


def edge_id(value):
    if not isinstance(value, tuple) or len(value) != 2:
        raise ValueError("native physical path/fragment identity required")
    node_id(value[0])
    if type(value[1]) is not int or not 0 <= value[1] < 2**32:
        raise ValueError("normalized fragment ordinal must be uint32")


def geographic(value):
    if (
        not isinstance(value, tuple)
        or len(value) != 2
        or any(type(v) not in (int, float) or not isfinite(v) for v in value)
        or abs(value[0]) > 180
        or abs(value[1]) > 90
    ):
        raise ValueError("finite immutable longitude/latitude required")


@dataclass(frozen=True, slots=True)
class PortNode:
    identifier: int
    longitude_latitude: tuple[float, float]

    def __post_init__(self):
        node_id(self.identifier)
        geographic(self.longitude_latitude)


@dataclass(frozen=True, slots=True)
class PortSpan:
    """One physical normalized edge, ordered in the actual source direction.

    The fragment ordinal belongs to the native edge ID. Original source-segment
    ordinals, source tables, supplied/derived fields and aliases remain attributes;
    they are not reconstructed or equated to original OSM identities.
    """

    identifier: tuple[int, int]
    nodes: tuple[int, int]
    width_m: float
    maximum_gradient: float | None
    bridge: bool
    tunnel: bool
    attributes: Mapping[str, object]
    layer: int = 0

    def __post_init__(self):
        edge_id(self.identifier)
        if not isinstance(self.attributes, Mapping):
            raise TypeError("immutable source metadata mapping required")
        if not isinstance(self.nodes, tuple) or len(self.nodes) != 2:
            raise ValueError("two original normalized source nodes required")
        for value in self.nodes:
            node_id(value)
        if self.nodes[0] == self.nodes[1]:
            raise ValueError("a physical source span needs two distinct nodes")
        if not isfinite(self.width_m) or not 0 <= self.width_m <= 1000:
            raise ValueError("finite bounded native width required")
        if self.maximum_gradient is not None and (
            not isfinite(self.maximum_gradient) or not 0 < self.maximum_gradient <= 10
        ):
            raise ValueError(
                "bounded positive grade recipe or explicit missing value required"
            )
        if type(self.bridge) is not bool or type(self.tunnel) is not bool:
            raise ValueError("explicit native bridge/tunnel flags required")
        if self.bridge and self.tunnel:
            raise ValueError("a normalized span cannot be both bridge and tunnel")
        if type(self.layer) is not int or not -(2**31) <= self.layer < 2**31:
            raise ValueError("actual normalized int32 source layer required")
        object.__setattr__(self, "attributes", immutable(self.attributes))


@dataclass(frozen=True, slots=True)
class PortSource:
    nodes: tuple[PortNode, ...]
    spans: tuple[PortSpan, ...]
    unknown_nodes: tuple[int, ...]
    binding: Mapping[str, object]

    def __post_init__(self):
        if not isinstance(self.binding, Mapping) or not self.binding:
            raise ValueError("explicit normalized source binding required")
        for value, kind in ((self.nodes, PortNode), (self.spans, PortSpan)):
            if not isinstance(value, tuple) or any(
                not isinstance(v, kind) for v in value
            ):
                raise ValueError("immutable normalized node/span tuples required")
        if not isinstance(self.unknown_nodes, tuple):
            raise TypeError("explicit immutable unknown source nodes required")
        for value in self.unknown_nodes:
            node_id(value)
        object.__setattr__(self, "binding", immutable(self.binding))


@dataclass(frozen=True, slots=True)
class PortDemand:
    """An actual source cut, normally supplied by SelectTransportCore.

    Boundary is (level, axis, line, segment), with longitude=0 and latitude=1.
    Each corner boundary may carry its own demand with the same canonical anchor.
    Core identity and request order do not enter the source field construction.
    """

    edge: tuple[int, int]
    fraction: float
    anchor_longitude_latitude: tuple[float, float]
    boundary: tuple[int, int, int, int]

    def __post_init__(self):
        edge_id(self.edge)
        geographic(self.anchor_longitude_latitude)
        if not isfinite(self.fraction) or not 0 <= self.fraction <= 1:
            raise ValueError("bounded original normalized edge fraction required")
        if (
            not isinstance(self.boundary, tuple)
            or len(self.boundary) != 4
            or any(type(v) is not int for v in self.boundary)
        ):
            raise ValueError("explicit native TransportBoundary identity required")
        level, axis, line, segment = self.boundary
        if not 0 <= level <= 24 or axis not in (0, 1):
            raise ValueError("valid native geographic boundary level/axis required")
        cells = 2**level
        if not 0 <= segment < cells or not 0 <= line < cells + axis:
            raise ValueError("valid native boundary line/segment required")


@dataclass(frozen=True, slots=True)
class PortBudget:
    input_spans: int = 8192
    input_nodes: int = 16384
    selected_spans: int = 2048
    incidence_visits: int = 65536
    requests: int = 128
    terrain_samples: int = 4096
    surface: PortHeightBudget = field(default_factory=PortHeightBudget)

    def __post_init__(self):
        for name in (
            "input_spans",
            "input_nodes",
            "selected_spans",
            "incidence_visits",
            "requests",
            "terrain_samples",
        ):
            value = getattr(self, name)
            if type(value) is not int or not 0 < value <= 1048576:
                raise ValueError("bounded positive integer port-work limits required")
        if not isinstance(self.surface, PortHeightBudget):
            raise TypeError("existing PortHeightBudget required")


@dataclass(frozen=True, slots=True)
class PortValue:
    edge: tuple[int, int]
    boundary: tuple[int, int, int, int]
    anchor_longitude_latitude: tuple[float, float]
    height_m: float
    direction_en: tuple[float, float]
    plane_en: tuple[float, float]
    half_width_m: float
    longitudinal_grade: float
    crossfall_rad: float
    source_attributes: Mapping[str, object]


@dataclass(frozen=True, slots=True)
class PortResult:
    """Values align with demands; a missing value always has an explicit reason."""

    values: tuple[PortValue | None, ...]
    reasons: tuple[str, ...]
    report: Mapping[str, object]

    def __post_init__(self):
        if len(self.values) != len(self.reasons) or any(
            (value is None) != bool(reason)
            for value, reason in zip(self.values, self.reasons, strict=True)
        ):
            raise ValueError("each missing port needs its own explicit reason")
        object.__setattr__(self, "report", immutable(self.report))
