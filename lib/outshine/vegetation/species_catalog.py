"""Fixed pilot taxonomy and regional occurrence candidates, without ecology claims."""

import json
from dataclasses import dataclass
from importlib.resources import files
from types import MappingProxyType
from typing import Mapping

from ..contracts import immutable

FIELDS = ("region", "introduced", "extinct", "doubtful", "source_ids")


def region_code(code):
    if (
        not isinstance(code, str)
        or len(code) != 3
        or not code.isascii()
        or not code.isalpha()
        or not code.isupper()
    ):
        raise ValueError("WGSRPD level3 code required")
    return code


def occurrence_index(records):
    native, introduced = {}, {}
    for taxon, record in records.items():
        for row in record["distribution"]:
            if len(row) != len(FIELDS) or any(
                type(v) is not int or v not in (0, 1) for v in row[1:4]
            ):
                raise ValueError("independent binary occurrence flags required")
            region_code(row[0])
            if not row[4] or any(not isinstance(v, str) or not v for v in row[4]):
                raise ValueError("retained occurrence source IDs required")
            if not row[2] and not row[3]:
                target = introduced if row[1] else native
                target.setdefault(row[0], set()).add(taxon)
    return tuple(
        MappingProxyType({k: tuple(sorted(v)) for k, v in index.items()})
        for index in (native, introduced)
    )


@dataclass(frozen=True, slots=True, init=False)
class SpeciesCatalog:
    """Immutable explicit species metadata; an empty pool is not absence of flora.

    Construction accepts a trusted structured recipe, not an untrusted asset ABI.
    Recorded extant/certain native and introduced occurrences stay separate.
    Candidate lists say neither abundance nor planting or morphology readiness.
    """

    document: Mapping
    records: Mapping[str, Mapping]
    aliases: Mapping[str, str]
    _native: Mapping[str, tuple[str, ...]]
    _introduced: Mapping[str, tuple[str, ...]]

    def __init__(self, document):
        if (
            document["format"] != "outshine-species-pilot-1"
            or tuple(document["distribution_fields"]) != FIELDS
        ):
            raise ValueError(
                "explicit species pilot format and occurrence columns required"
            )
        if not 1 <= len(document["species"]) <= 2048:
            raise ValueError("bounded pilot species required")
        document = immutable(document)
        records = {}
        for record in document["species"]:
            key = record["id"]
            if key != "outshine:taxon:wcvp:" + record["wcvp_id"] or key in records:
                raise ValueError("unique namespaced taxon identity required")
            if not record["scientific_name"]:
                raise ValueError("explicit scientific species name required")
            records[key] = record
        aliases = {}
        for request in document["requests"]:
            name, key = request["requested_name"], request["accepted_id"]
            if not name or key not in records or name in aliases:
                raise ValueError(
                    "unique explicit request referring to a present species required"
                )
            aliases[name] = key
        native, introduced = occurrence_index(records)
        for name, value in (
            ("document", document),
            ("records", MappingProxyType(records)),
            ("aliases", MappingProxyType(aliases)),
            ("_native", native),
            ("_introduced", introduced),
        ):
            object.__setattr__(self, name, value)

    def candidate_ids(
        self, regions: tuple[str, ...], *, introduced: bool = False
    ) -> tuple[str, ...]:
        """Union region candidates; True selects introduced records only.

        Extinct/doubtful entries stay in metadata but are excluded here. Unknown
        valid region codes return no pilot candidate, not a barren-world claim.
        """
        if (
            not isinstance(regions, tuple)
            or len(regions) > 1024
            or type(introduced) is not bool
        ):
            raise ValueError(
                "bounded explicit region tuple and introduced flag required"
            )
        index = self._introduced if introduced else self._native
        result = set()
        for region in regions:
            result.update(index.get(region_code(region), ()))
        return tuple(sorted(result))


def load_species_catalog() -> SpeciesCatalog:
    """Read the pinned packaged pilot once; caller owns/reuses the result."""
    source = files(__package__) / "data" / "species-pilot-1.json"
    return SpeciesCatalog(json.loads(source.read_text(encoding="utf-8")))
