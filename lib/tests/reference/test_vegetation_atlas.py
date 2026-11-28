import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from outshine.vegetation.atlas import (
    COMMUNITY_DTYPE,
    NO_SOIL,
    OBSERVATION_DTYPE,
    RASTER_DTYPES,
    TILE_CELLS,
    VegetationAtlas,
    grid_cell,
    tile_bounds,
    tile_name,
)
from outshine.vegetation.species_catalog import species_catalog_digest
from shapely.geometry import box


class VegetationAtlasTest(unittest.TestCase):
    def build_fixture(self, root, key, *, community=False, observed=False):
        values = {
            "cover": 10,
            "ecoregion": 265,
            "botanical": 1,
            "sand": 422,
            "silt": NO_SOIL,
            "clay": 125,
            "cfvo": 37,
            "soc": 204,
        }
        rasters = {
            name: np.full((TILE_CELLS, TILE_CELLS), values[name], dtype=dtype)
            for name, dtype in RASTER_DTYPES.items()
        }
        if community:
            rasters["community"] = np.ones(
                (TILE_CELLS, TILE_CELLS), dtype=COMMUNITY_DTYPE
            )
        if observed:
            rasters["observation"] = np.full(
                (TILE_CELLS, TILE_CELLS), 2, dtype=OBSERVATION_DTYPE
            )
        temporary = root / "tile.npz"
        np.savez_compressed(temporary, **rasters)
        digest = hashlib.sha256(temporary.read_bytes()).hexdigest()
        name = tile_name(key)
        product = temporary.rename(root / f"{name}-{digest}.npz")
        sources = {"species": {"catalog_sha256": species_catalog_digest()}}
        tile = {"sha256": digest}
        if community:
            tile["potential_community"] = True
        if observed:
            tile["observed_vegetation"] = True
        (root / "atlas.json").write_text(
            json.dumps(
                {
                    "format": "outshine-vegetation-atlas-1",
                    "sources": sources,
                    "ecoregions": {
                        "265": ["Peninsular Malaysian rain forests", 1, "IM"]
                    },
                    "botanical_regions": ["", "MLY"],
                    "potential_communities": ["", "CHL-2017:P70"]
                    if community
                    else [""],
                    "observations": [
                        {
                            "map_unit": "JOTR-2012:29032",
                            "label": "Yucca schidigera - Coleogyne ramosissima",
                            "height_class": 2,
                            "density_class": density,
                        }
                        for density in (4, 5)
                    ]
                    if observed
                    else [],
                    "tiles": {name: tile},
                }
            )
        )
        return product

    def test_observation_raster_has_its_own_classes_and_preserves_unknown(self):
        latitude, longitude = 34.01348287, -116.16867665
        with tempfile.TemporaryDirectory() as root:
            self.build_fixture(
                Path(root),
                grid_cell(latitude, longitude)[0],
                community=True,
                observed=True,
            )
            atlas = VegetationAtlas(root)
            sample = atlas.sample(latitude, longitude)
            self.assertEqual(sample.potential_community, "CHL-2017:P70")
            self.assertEqual(sample.observed_vegetation.map_unit, "JOTR-2012:29032")
            self.assertEqual(sample.observed_vegetation.height_class, 2)
            indices, density = atlas.observed_cells(
                latitude, [longitude, longitude + 0.1]
            )
            np.testing.assert_array_equal(indices, [2, 0])
            np.testing.assert_array_equal(density, [5, 0])
            np.testing.assert_array_equal(
                atlas.observation_mask(indices, ("JOTR-2012:29032",)), [True, False]
            )
            self.assertIsNone(
                atlas.sample(latitude, longitude + 0.1).observed_vegetation
            )

    def test_geographic_index_owns_shared_edges_once_and_wraps_antimeridian(self):
        key, _ = grid_cell(48.19054, 16.47658)
        _, south, east, north = tile_bounds(key)
        latitude = (south + north) / 2
        self.assertEqual(grid_cell(latitude, east - 1e-9)[0], key)
        self.assertEqual(grid_cell(latitude, east)[0], (key[0] + 1, key[1]))
        self.assertEqual(grid_cell(latitude, east + 1e-9)[0], (key[0] + 1, key[1]))
        self.assertEqual(grid_cell(0, -180), grid_cell(0, 180))

    def test_reopened_atlas_joins_regional_and_mapped_evidence(self):
        latitude, longitude = 1.35331, 103.77873
        with tempfile.TemporaryDirectory() as root:
            self.build_fixture(Path(root), grid_cell(latitude, longitude)[0])
            atlas = VegetationAtlas(root)
            observation = atlas.sample(latitude, longitude)
            self.assertEqual(observation.cover, "wood")
            self.assertEqual(observation.ecoregion_id, 265)
            self.assertEqual(observation.biome_id, 1)
            self.assertIn("evergreen broadleaf trees", observation.growth_forms)
            self.assertEqual(observation.botanical_region, "MLY")
            self.assertEqual(
                observation.native_candidates, atlas.species.candidate_ids(("MLY",))
            )
            self.assertEqual(observation.soil.sand_g_kg, 422)
            self.assertIsNone(observation.soil.silt_g_kg)
            self.assertEqual(observation.soil.coarse_fragments_pct, 3.7)
            self.assertEqual(observation.soil.organic_carbon_g_kg, 20.4)
            scene = SimpleNamespace(
                lat=latitude,
                lon=longitude,
                metres=np.array([111000, 111000]),
                features=[("landcover", box(-5, -5, 5, 5), {"class": "wetland"})],
            )
            local = atlas.sample_local(scene, (0, 0))
            self.assertEqual(local.cover_source, "mapped")
            self.assertEqual(local.cover, "wetland")
            self.assertEqual(local.worldcover_code, 10)
            self.assertNotIn("evergreen broadleaf trees", local.growth_forms)
            absent = atlas.sample(-50.97, -72.80)
            self.assertIsNone(absent.cover)
            self.assertIsNone(absent.soil)
            self.assertEqual(absent.growth_forms, ())

    def test_corrupt_tile_is_a_detectable_error(self):
        with tempfile.TemporaryDirectory() as root:
            product = self.build_fixture(Path(root), grid_cell(0, 0)[0])
            product.write_bytes(b"wrong payload")
            with self.assertRaisesRegex(ValueError, "changed vegetation atlas tile"):
                VegetationAtlas(root).sample(0, 0)

    def test_batch_reads_share_scalar_tile_ownership(self):
        with tempfile.TemporaryDirectory() as root:
            self.build_fixture(Path(root), grid_cell(1.35, 103.77)[0])
            atlas = VegetationAtlas(root)
            longitude = [[103.77, 103.80], [103.76, 103.75]]
            cover, ecoregion = atlas.land_cells(1.35, longitude)
            np.testing.assert_array_equal(cover, [[10, 0], [10, 0]])
            np.testing.assert_array_equal(ecoregion, [[265, 0], [265, 0]])
            for row, values in enumerate(longitude):
                for column, lon in enumerate(values):
                    sample = atlas.sample(1.35, lon)
                    self.assertEqual(cover[row, column], sample.worldcover_code or 0)
                    self.assertEqual(ecoregion[row, column], sample.ecoregion_id or 0)

    def test_potential_community_is_independent_of_observed_cover_and_missing_tiles(
        self,
    ):
        with tempfile.TemporaryDirectory() as root:
            latitude, longitude = -50.98, -72.80
            self.build_fixture(
                Path(root), grid_cell(latitude, longitude)[0], community=True
            )
            atlas = VegetationAtlas(root)
            sample = atlas.sample(latitude, longitude, mapped_cover="water")
            self.assertEqual(sample.potential_community, "CHL-2017:P70")
            self.assertEqual(sample.cover, "water")
            np.testing.assert_array_equal(
                atlas.community_mask(
                    latitude, [longitude, longitude + 1], ("CHL-2017:P70",)
                ),
                [True, False],
            )
            self.assertIsNone(atlas.sample(latitude, longitude + 1).potential_community)
