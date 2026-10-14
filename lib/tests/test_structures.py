import dataclasses
import unittest

import shapely

from outshine_ref.buildings.structures import classify_structure, classify_structure_marker


class StructuralInterpretationTests(unittest.TestCase):
    def test_poi_marker_classifies_no_dimensions_or_ownership(self):
        marker=classify_structure_marker({'class':'historic','subclass':'monument','name':'Hotel'})
        self.assertEqual(marker.kind,'monument')
        self.assertEqual(marker.source_field,'subclass')
        self.assertFalse(hasattr(marker,'height_m'))
        self.assertIsNone(classify_structure_marker({'name':'Chimney','class':'attraction'}))
        self.assertEqual(classify_structure_marker({'class':'mast','subclass':'chimney'}).kind,'ambiguous')
        with self.assertRaises(ValueError):
            classify_structure_marker({'subclass':['chimney']})

    def test_compact_slender_form_has_alternatives_and_ignores_names(self):
        footprint=shapely.box(0,0,2,2)
        a=classify_structure(footprint, {'render_height':50,'name':'Residential House'})
        b=classify_structure(footprint, {'render_height':50,'name':'Cooling Tower','id':'unrelated'})
        self.assertEqual(a,b)
        self.assertEqual(a.kind,'mast')
        self.assertEqual(a.origin,'shape')
        self.assertIn('chimney',a.alternatives)
        with self.assertRaises(dataclasses.FrozenInstanceError):
            a.kind='chimney'

    def test_industrial_shaft_and_unproven_type(self):
        shaft=shapely.Point(0,0).buffer(6,quad_segs=8)
        value=classify_structure(shaft, {'render_height':121}, industrial_coverage=1.)
        self.assertEqual(value.kind,'chimney')
        self.assertEqual(value.origin,'shape-context')
        self.assertIn('silo',value.alternatives)
        urban=classify_structure(shaft, {'render_height':121})
        self.assertEqual(urban.kind,'tower')
        self.assertIn('water_tower',urban.alternatives)

    def test_large_industrial_round_form_distinguishes_height_regime(self):
        footprint=shapely.Point(0,0).buffer(50,quad_segs=16)
        a=classify_structure(footprint, {'render_height':138}, industrial_coverage=1.)
        b=classify_structure(footprint, {'render_height':56}, industrial_coverage=1.)
        c=classify_structure(footprint, {'render_height':138})
        self.assertEqual(a.kind,'cooling_tower')
        self.assertEqual(b.kind,'industrial_round_body')
        self.assertIn('reactor_building',b.alternatives)
        self.assertEqual(c.kind,'ordinary')

    def test_high_narrow_houses_and_high_rise_keep_building_programme(self):
        for footprint,height in [(shapely.box(0,0,3,16),30), (shapely.box(0,0,8,8),60),
                                 (shapely.box(0,0,20,20),300), (shapely.Point(0,0).buffer(30),180)]:
            self.assertEqual(classify_structure(footprint, {'render_height':height}).kind,'ordinary')

    def test_extremely_tall_wide_round_shaft_keeps_high_rise_alternative(self):
        d=classify_structure(shapely.Point(0,0).buffer(28), {'render_height':634})
        self.assertEqual(d.kind,'tower')
        self.assertIn('round_highrise',d.alternatives)

    def test_clips_and_courtyards_do_not_establish_special_form(self):
        thin=shapely.box(0,0,2,2)
        d=classify_structure(thin, {'render_height':50}, clipped=True)
        self.assertEqual(d.kind,'ordinary')
        self.assertEqual(d.origin,'incomplete-geometry')
        court=shapely.Point(0,0).buffer(10).difference(shapely.Point(0,0).buffer(5))
        self.assertEqual(classify_structure(court, {'render_height':100}).kind,'ordinary')

    def test_elevated_round_component_retains_absolute_vertical_interval(self):
        p=shapely.Point(0,0).buffer(17.5,quad_segs=16)
        value=classify_structure(p, {'render_min_height':290,'render_height':350})
        self.assertEqual(value.kind,'tower_section')
        self.assertEqual(value.metrics.bottom_m,290)
        self.assertEqual(value.metrics.top_m,350)
        self.assertEqual(value.metrics.height_m,60)

    def test_explicit_provider_class_and_suppression_precede_geometry(self):
        p=shapely.box(0,0,2,2)
        for kind in ('water_tower','satellite_dish','wind_turbine','monument','greenhouse'):
            d=classify_structure(p, {'render_height':50,'subclass':kind})
            self.assertEqual(d.kind,kind)
            self.assertEqual(d.origin,'provider')
            self.assertEqual(d.source_field,'subclass')
        self.assertEqual(classify_structure(p, {'render_height':50,'subclass':'chimney','hide_3d':True}).kind,'omit')
        d=classify_structure(p, {'render_height':50,'subclass':'chimney','class':'water_tower'})
        self.assertEqual(d.origin,'ambiguous-provider')
        self.assertEqual(set(d.alternatives),{'chimney','water_tower'})

    def test_quantization_cannot_make_borderline_footprint_certifiably_small(self):
        p=shapely.box(0,0,3.4,3.4)
        self.assertEqual(classify_structure(p, {'render_height':50}).kind,'mast')
        self.assertEqual(classify_structure(p, {'render_height':50},coordinate_step_m=.4).kind,'ordinary')

    def test_invalid_coordinates_heights_and_context_are_rejected(self):
        p=shapely.box(0,0,2,2)
        for properties in ({'render_height':0},{'render_height':float('nan')},{'render_height':True},
                           {'render_height':9,'render_min_height':10},{'hide_3d':'invalid'}):
            with self.assertRaises(ValueError):
                classify_structure(p,properties)
        for kwargs in ({'coordinate_step_m':-1},{'industrial_coverage':1.1},{'coordinate_step_m':True}):
            with self.assertRaises(ValueError):
                classify_structure(p,{},**kwargs)
        with self.assertRaises(ValueError):
            classify_structure(shapely.Point(0,0),{})


if __name__=='__main__':
    unittest.main()
