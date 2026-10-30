"""Black-box regression tests of the compiled C++ executable."""
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv.pop(1)).resolve()


class NativeCliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="rpg2lx native tests ")
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name) / "game with spaces"
        shutil.copytree(ROOT / "fixtures/valid_project", self.game)

    def run_cli(self, *args):
        return subprocess.run([str(BINARY), *map(str, args)], capture_output=True,
                              encoding="utf-8", errors="replace")

    def test_valid_project_and_map(self):
        self.assertEqual(self.run_cli("inspect", self.game).returncode, 0)
        result = self.run_cli("inspect-map", self.game / "data/Map001.json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Cells: 1326", result.stdout)

    def test_invalid_json_in_each_required_file(self):
        for name in ("System", "MapInfos", "Actors", "CommonEvents"):
            path = self.game / f"data/{name}.json"
            original = path.read_bytes()
            with self.subTest(name=name):
                path.write_text('{"x":01}', encoding="utf-8")
                self.assertEqual(self.run_cli("inspect", self.game).returncode, 2)
            path.write_bytes(original)

    def test_unreadable_data_is_not_valid(self):
        path = self.game / "data/Actors.json"
        path.unlink()
        path.mkdir()
        self.assertEqual(self.run_cli("inspect", self.game).returncode, 2)

    def test_semantic_system_errors(self):
        path = self.game / "data/System.json"
        data = json.loads(path.read_text())
        for bad in (-1, 1.5, "1", 1e30):
            with self.subTest(startMapId=bad):
                path.write_text(json.dumps(dict(data, startMapId=bad)))
                self.assertEqual(self.run_cli("inspect", self.game).returncode, 2)

    def test_unicode_title(self):
        path = self.game / "data/System.json"
        data = json.loads(path.read_text())
        path.write_text(json.dumps(dict(data, gameTitle="Espa\u00f1a \U0001f600")))
        result = self.run_cli("inspect", self.game)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Espa\u00f1a \U0001f600", result.stdout)

    def test_bad_map_and_arguments(self):
        path = self.game / "data/Map001.json"
        data = json.loads(path.read_text())
        data["data"] = [0]
        path.write_text(json.dumps(data))
        self.assertEqual(self.run_cli("inspect-map", path).returncode, 2)
        self.assertEqual(self.run_cli("inspect", self.game, "extra").returncode, 1)
        self.assertEqual(self.run_cli("unknown-command").returncode, 1)

    def test_static_plugin_configuration(self):
        path = self.game / "js/plugins.js"
        path.write_text('// comment with [brackets]\nvar $plugins = ' + json.dumps([
            {"name": "Disabled", "status": False, "parameters": {"name": "not a plugin"}},
            {"name": "Active", "status": True, "parameters": {"value": "[text]"}}
        ]) + '; // trailing comment\n', encoding="utf-8")
        result = self.run_cli("inspect", self.game)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("Disabled", result.stdout)
        self.assertIn("Detected Plugins: 2", result.stdout)
        self.assertNotIn("  - not a plugin", result.stdout)
        path.write_text('var $plugins = []; runUnknownCode();', encoding="utf-8")
        self.assertEqual(self.run_cli("inspect", self.game).returncode, 2)

    def test_library_layout_and_multiple_games(self):
        home = Path(self.temp.name) / "RPG2LX home"
        result = self.run_cli("list", "--home", home)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("RPG2LX games: 0", result.stdout)
        for folder in ("games", "saves", "config/games", "logs"):
            self.assertTrue((home / folder).is_dir())
        shutil.copytree(self.game, home / "games/First Game")
        shutil.copytree(self.game, home / "games/Second Game/www")
        (home / "games/Incomplete").mkdir()
        result = self.run_cli("list", "--home", home)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("RPG2LX games: 3", result.stdout)
        self.assertEqual(result.stdout.count("[MV]"), 2)
        self.assertIn("[INCOMPLETE]", result.stdout)
        self.assertFalse((home / "games/First Game/saves").exists())
        self.assertEqual(self.run_cli("list", "--home").returncode, 1)

    def test_library_directory_collision(self):
        home = Path(self.temp.name) / "collision"
        home.mkdir()
        (home / "games").write_text("This is a file")
        result = self.run_cli("list", "--home", home)
        self.assertEqual(result.returncode, 2)
        self.assertEqual((home / "games").read_text(), "This is a file")

    def test_unicode_game_folder(self):
        unicode_game = Path(self.temp.name) / "Aventura de Luc\u00eda"
        shutil.copytree(self.game, unicode_game)
        result = self.run_cli("inspect", unicode_game)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_inspect_export_root_or_entry_file(self):
        export = Path(self.temp.name) / "Windows export"
        shutil.move(self.game, export / "www")
        (export / "Game.exe").write_bytes(b"Not executed: entry path only")
        for source in (export, export / "Game.exe", export / "www"):
            with self.subTest(source=source):
                result = self.run_cli("inspect", source)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(str(export / "www"), result.stdout)

    def test_engine_identity_is_read_from_export(self):
        core = self.game / "js/rpg_core.js"
        core.write_text("Utils.RPGMAKER_NAME = 'MV';\nUtils.RPGMAKER_VERSION = '1.6.2';\n")
        result = self.run_cli("inspect", self.game)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("MV Version: 1.6.2", result.stdout)
        core.unlink()
        (self.game / "js/rmmz_core.js").write_text("// MZ marker")
        result = self.run_cli("inspect", self.game)
        self.assertEqual(result.returncode, 2)
        self.assertIn("MZ", result.stdout)


if __name__ == "__main__":
    unittest.main()
