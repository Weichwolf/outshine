"""Bounded exact empty-band certificate, without rounding constructed XY vertices."""

from fractions import Fraction

import shapely
from stair_occupancy_geometry import OccupancyError, clip_chain, plane_value, rational

MAX_EXACT_VISITS = 16384


def debit(remaining, count):
    if count > remaining:
        raise OccupancyError("exact empty-band vertex budget exhausted")
    return remaining - count


def exact_band_area(reservation, heights, *, max_visits=MAX_EXACT_VISITS):
    """Signed ring integrals retain holes and disconnected clipped components."""
    if type(max_visits) is not int or not 1 <= max_visits <= MAX_EXACT_VISITS:
        raise ValueError("bounded exact empty-band capacity required")
    if shapely.get_num_coordinates(reservation.geometry) > max_visits:
        raise OccupancyError("exact empty-band vertex budget exhausted")
    remaining, total = max_visits, Fraction(0)
    for polygon in shapely.get_parts(reservation.geometry):
        for index, ring in enumerate((polygon.exterior, *polygon.interiors)):
            remaining = debit(remaining, len(ring.coords))
            chain = [tuple(map(rational, xy)) for xy in list(ring.coords)[:-1]]
            for plane, limit, sign in (
                (reservation.lower_plane_m, heights.upper_m, -1),
                (reservation.upper_plane_m, heights.lower_m, 1),
            ):
                if not chain:
                    break
                remaining = debit(remaining, len(chain))
                values = [
                    sign
                    * (plane_value(plane, reservation.origin_xy_m, p) - rational(limit))
                    for p in chain
                ]
                chain = clip_chain(chain, values)
            remaining = debit(remaining, len(chain))
            area = (
                abs(
                    sum(
                        (
                            a[0] * b[1] - b[0] * a[1]
                            for a, b in zip(chain, chain[1:] + chain[:1], strict=True)
                        ),
                        Fraction(0),
                    )
                )
                / 2
            )
            total += area if index == 0 else -area
    if total < 0:
        raise OccupancyError(
            "exact empty-band ring areas disagree with the valid domain"
        )
    return total


def certified_empty(reservation, heights):
    if exact_band_area(reservation, heights) > 0:
        raise OccupancyError(
            "positive affine conflict is not representable at the current XY precision"
        )
    return shapely.Polygon()
