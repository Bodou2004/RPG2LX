#!/usr/bin/env python3
"""
Unit test suite verifying Inspector behavior across all fixtures:
- valid_project (with Map001.json, multiple DB entities, package.json, plugins)
- incomplete_project (missing core files)
- invalid_json_project (unclosed syntax)
- invalid_json_balanced (syntax errors with balanced braces)
- type_mismatch_project (string instead of object, object instead of array)
- path with spaces (spaces in filesystem path)
"""
import os
import unittest
from rpg2lx_ref import ProjectInspector

class TestRPG2LXInspector(unittest.TestCase):
    def setUp(self):
        self.base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.fixtures_dir = os.path.join(self.base_dir, "fixtures")

    def test_valid_project(self):
        valid_path = os.path.join(self.fixtures_dir, "valid_project")
        meta = ProjectInspector.inspect(valid_path)
        self.assertTrue(meta["is_valid"])
        self.assertEqual(meta["game_title"], "Test Adventure")
        self.assertEqual(meta["start_map_id"], "1")
        self.assertEqual(meta["screen_width"], 816)
        self.assertEqual(meta["screen_height"], 624)
        self.assertEqual(len(meta["plugins"]), 2)
        self.assertEqual(meta["plugins"][0]["name"], "Community_Basic")
        self.assertTrue(meta["plugins"][0]["status"])
        self.assertIn("cacheLimit", meta["plugins"][0]["parameters"])
        self.assertEqual(meta["actor_count"], 1)
        self.assertEqual(meta["map_count"], 1)
        self.assertEqual(meta["common_event_count"], 1)
        self.assertEqual(len(meta["missing_files"]), 0)
        self.assertEqual(len(meta["warnings"]), 0)

    def test_incomplete_project(self):
        inc_path = os.path.join(self.fixtures_dir, "incomplete_project")
        meta = ProjectInspector.inspect(inc_path)
        self.assertFalse(meta["is_valid"])
        self.assertIn(os.path.join("data", "MapInfos.json"), meta["missing_files"])
        self.assertIn(os.path.join("data", "Actors.json"), meta["missing_files"])
        self.assertIn(os.path.join("data", "CommonEvents.json"), meta["missing_files"])

    def test_invalid_json_project(self):
        inv_path = os.path.join(self.fixtures_dir, "invalid_json_project")
        meta = ProjectInspector.inspect(inv_path)
        self.assertFalse(meta["is_valid"])
        self.assertTrue(any("System.json syntax error" in w for w in meta["warnings"]))

    def test_invalid_json_balanced(self):
        inv_bal_path = os.path.join(self.fixtures_dir, "invalid_json_balanced")
        meta = ProjectInspector.inspect(inv_bal_path)
        self.assertFalse(meta["is_valid"])
        self.assertTrue(any("System.json syntax error" in w for w in meta["warnings"]))

    def test_type_mismatch_project(self):
        type_path = os.path.join(self.fixtures_dir, "type_mismatch_project")
        meta = ProjectInspector.inspect(type_path)
        self.assertFalse(meta["is_valid"])
        self.assertTrue(len(meta["warnings"]) > 0)

    def test_path_with_spaces(self):
        spaces_path = os.path.join(self.fixtures_dir, "path with spaces")
        meta = ProjectInspector.inspect(spaces_path)
        self.assertTrue(meta["is_valid"])
        self.assertEqual(meta["game_title"], "Test Adventure")

if __name__ == "__main__":
    unittest.main()
