"""Repeated-operation tags, stable leaf identities, empty results and complete ancestry."""

from dataclasses import replace
import unittest

import numpy as np
import shapely

from boolean_sequence import leaf,combine,Reference
from verify_boolean_sequence import source,verify,room
from verify_boolean_surfaces import Prism


class SequenceControls(unittest.TestCase):
    def setUp(self):
        self.prism=Prism(shapely.box(0,0,3,4),0.,5.)
        self.a=source('a',self.prism,0)

    def test_repeated_leaf_preserves_one_original_source_and_two_current_operands(self):
        node=combine('same-leaf',self.a,self.a)
        verify(node,(60.,94.));self.assertIs(node.operand.parent,node)
        self.assertTrue(all(len(row)==1 for row in node.origins))
        self.assertTrue(all({'bool.A','bool.B'}<=tags for tags in node.product.attributes.tags))

    def test_distinct_ids_on_identical_geometry_keep_both_and_a_priority(self):
        b=source('b',self.prism,1)
        for first,last in ((self.a,b),(b,self.a)):
            node=combine('priority-'+first.leaves[0].identifier,first,last)
            verify(node,(60.,94.))
            self.assertTrue(all({reference.source for reference in row}=={'a','b'} for row in node.origins))
            self.assertTrue(all(reference.source==first.leaves[0].identifier for reference in node.preferred))

    def test_duplicate_declared_ids_are_rejected(self):
        b=source('a',self.prism,1)
        with self.assertRaisesRegex(ValueError,'Duplicate CSG leaf identity'):combine('ambiguous',self.a,b)

    def test_previous_bool_tags_replace_only_reserved_tags(self):
        shape=self.a.shape
        faces=tuple(replace(face,tags=face.tags|{'bool.B','bool.cut','bool.custom','user.entrance'}) for face in shape.geometry.polygons)
        a=leaf('tagged',replace(shape,geometry=replace(shape.geometry,polygons=faces)))
        b=source('remote',Prism(shapely.box(10,0,13,4),0.,5.),1)
        node=combine('new-tags',a,b)
        for preferred,tags in zip(node.preferred,node.product.attributes.tags,strict=True):
            if preferred.source=='tagged':
                self.assertIn('bool.A',tags);self.assertNotIn('bool.B',tags);self.assertNotIn('bool.cut',tags)
                self.assertTrue({'bool.custom','user.entrance'}<=tags)
        self.assertTrue(all('bool.B' in face.tags for face in a.shape.geometry.polygons))

    def test_empty_intermediates_follow_all_boolean_identities(self):
        empty=combine('empty',self.a,self.a,'subtract').operand
        for operation in ('union','subtract','intersect'):
            for first,last in ((empty,self.a),(self.a,empty),(empty,empty)):
                node=combine('empty-'+operation,first,last,operation)
                nonempty=(operation=='union' and (first is self.a or last is self.a)) or (operation=='subtract' and first is self.a)
                verify(node,(60.,94.) if nonempty else (0.,0.))

    def test_corrupted_recursive_relations_fail_before_geometry(self):
        bad=replace(self.a,preferred=(Reference('unknown',0,0),*self.a.preferred[1:]))
        with self.assertRaisesRegex(ValueError,'preferred origin'):combine('bad-preferred',bad,self.a)
        broken=replace(self.a,origins=self.a.origins[:-1])
        with self.assertRaisesRegex(ValueError,'incomplete'):combine('bad-length',broken,self.a)
        wrong=Reference('a',0,9999)
        broken=replace(self.a,origins=((wrong,),*self.a.origins[1:]),preferred=(wrong,*self.a.preferred[1:]))
        with self.assertRaisesRegex(ValueError,'invalid original'):combine('bad-triangle',broken,self.a)

    def test_room_chain_keeps_source_payloads_and_old_graph_tags(self):
        prisms,declared,cutter,shell,sections=room()
        self.assertEqual({ref.source for rows in shell.origins for ref in rows},{'outer','room','door'})
        self.assertIs(shell.parents[1].parent,cutter)
        self.assertTrue(any({'bool.A','bool.B'}<=tags for tags in cutter.product.attributes.tags))
        for supports,tags in zip(shell.product.overlay.supports,shell.product.attributes.tags,strict=True):
            self.assertEqual('bool.A' in tags,any(owner==0 for owner,_ in supports))
        self.assertTrue(all(row['route_wall_overlap_m2']<1e-7 for row in sections))

    def test_replay_uses_stable_explicit_references_and_preserves_inputs(self):
        before={key:value.copy() for key,value in self.a.shape.emit().items()}
        first=combine('replay',self.a,self.a)
        recreated=source('a',self.prism,0);last=combine('replay',recreated,recreated)
        self.assertEqual(first.origins,last.origins);self.assertEqual(first.preferred,last.preferred)
        for key,value in before.items():np.testing.assert_array_equal(self.a.shape.emit()[key],value)


if __name__=='__main__':unittest.main()
