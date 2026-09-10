#!/usr/bin/env python3
"""Independent positive and negative controls for the writer source inventory."""
import contextlib
import io
import unittest
from unittest.mock import patch

import grammar_vs_writer as inventory


class WriterInventory(unittest.TestCase):
    def check_inventory(self, source, declared, expected):
        with patch.object(inventory.grammar.WRITER.__class__, 'read_text', return_value=source), \
                patch.object(inventory.grammar, 'children', return_value=set(declared)), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(inventory.main(), expected)

    def test_multiple_tags_in_one_literal(self):
        self.check_inventory('out += "<lighting>\\n<key/>";', ['lighting', 'key'], 0)

    def test_adjacent_literals_form_one_tag(self):
        self.check_inventory('out += "<" /* split */ "clock" "/>";', ['clock'], 0)

    def test_raw_literal_with_delimiter(self):
        self.check_inventory('out += R"xml(<lighting name="x"><key/></lighting>)xml";',
                             ['lighting', 'key'], 0)

    def test_escaped_opening_and_encoding_prefix(self):
        self.check_inventory(r'out += u8"\x3c" "clock/>";', ['clock'], 0)

    def test_unsupported_escape_is_analysis_failure(self):
        self.check_inventory(r'out += "\x00003cclock/>";', ['clock'], 2)

    def test_comments_are_not_implementation(self):
        self.check_inventory('// "<clock/>"\n/* "<clock/>" */', ['clock'], 1)

    def test_character_literal_is_not_xml(self):
        self.check_inventory("auto x = '\"'; auto y = '<';", ['clock'], 1)

    def test_missing_section_is_detected(self):
        self.check_inventory('out += "<clock/>";', ['clock', 'lighting'], 1)

    def test_separate_statements_do_not_form_a_literal(self):
        self.check_inventory('out += "<"; out += "clock/>";', ['clock'], 1)

    def test_malformed_source_is_analysis_failure(self):
        self.check_inventory('out += "<clock/>', ['clock'], 2)

    def test_empty_grammar_is_analysis_failure(self):
        self.check_inventory('out += "<clock/>";', [], 2)


if __name__ == '__main__':
    unittest.main()
