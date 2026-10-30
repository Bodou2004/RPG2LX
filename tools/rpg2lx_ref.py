#!/usr/bin/env python3
"""
Python verification reference prototype mirroring rpg2lx core & CLI behavior
Strict parser adherence.
"""

import os
import sys
import json

class Logger:
    LEVELS = {"DEBUG": 0, "INFO": 1, "WARN": 2, "ERROR": 3}
    current_level = 1

    @classmethod
    def set_level(cls, level_name):
        cls.current_level = cls.LEVELS.get(level_name.upper(), 1)

    @classmethod
    def log(cls, level, msg):
        if cls.LEVELS.get(level, 1) >= cls.current_level:
            print(f"[{level}] {msg}")

    @classmethod
    def info(cls, msg): cls.log("INFO", msg)
    @classmethod
    def warn(cls, msg): cls.log("WARN", msg)
    @classmethod
    def error(cls, msg): cls.log("ERROR", msg)
    @classmethod
    def debug(cls, msg): cls.log("DEBUG", msg)


class ProjectInspector:
    @staticmethod
    def inspect(project_path):
        Logger.info(f"Inspecting RPG Maker MV project at: {project_path}")
        result = {
            "is_valid": False,
            "game_title": "",
            "start_map_id": "",
            "screen_width": 816,
            "screen_height": 624,
            "actor_count": 0,
            "map_count": 0,
            "common_event_count": 0,
            "plugins": [],
            "missing_files": [],
            "warnings": []
        }

        if not os.path.exists(project_path) or not os.path.isdir(project_path):
            Logger.error(f"Directory does not exist or cannot be accessed: {project_path}")
            result["warnings"].append(f"Directory does not exist: {project_path}")
            return result

        required_data = [
            os.path.join("data", "System.json"),
            os.path.join("data", "MapInfos.json"),
            os.path.join("data", "Actors.json"),
            os.path.join("data", "CommonEvents.json")
        ]

        for rel in required_data:
            full_path = os.path.join(project_path, rel)
            if not os.path.exists(full_path):
                result["missing_files"].append(rel)
                Logger.warn(f"Missing required RPG Maker MV file: {rel}")

        # Strict parse System.json
        system_json_path = os.path.join(project_path, "data", "System.json")
        if os.path.exists(system_json_path):
            try:
                with open(system_json_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    if not isinstance(data, dict):
                        result["warnings"].append("System.json root must be a JSON object")
                    else:
                        result["game_title"] = data.get("gameTitle", "")
                        result["start_map_id"] = str(data.get("startMapId", "1"))
                        Logger.info(f"Detected game title: {result['game_title'] or '(Untitled)'}")
            except Exception as e:
                Logger.error(f"System.json syntax error: {e}")
                result["warnings"].append(f"System.json syntax error: {e}")

        # Strict parse MapInfos.json
        map_infos_path = os.path.join(project_path, "data", "MapInfos.json")
        if os.path.exists(map_infos_path):
            try:
                with open(map_infos_path, "r", encoding="utf-8") as f:
                    maps = json.load(f)
                    if isinstance(maps, list):
                        result["map_count"] = sum(1 for m in maps if isinstance(m, dict))
                    else:
                        result["warnings"].append("MapInfos.json root must be an array")
            except Exception as e:
                result["warnings"].append(f"MapInfos.json syntax error: {e}")

        # Strict parse Actors.json
        actors_path = os.path.join(project_path, "data", "Actors.json")
        if os.path.exists(actors_path):
            try:
                with open(actors_path, "r", encoding="utf-8") as f:
                    actors = json.load(f)
                    if isinstance(actors, list):
                        result["actor_count"] = sum(1 for a in actors if isinstance(a, dict))
                    else:
                        result["warnings"].append("Actors.json root must be an array")
            except Exception as e:
                result["warnings"].append(f"Actors.json syntax error: {e}")

        # Strict parse CommonEvents.json
        common_events_path = os.path.join(project_path, "data", "CommonEvents.json")
        if os.path.exists(common_events_path):
            try:
                with open(common_events_path, "r", encoding="utf-8") as f:
                    cev = json.load(f)
                    if isinstance(cev, list):
                        result["common_event_count"] = sum(1 for c in cev if isinstance(c, dict))
                    else:
                        result["warnings"].append("CommonEvents.json root must be an array")
            except Exception as e:
                result["warnings"].append(f"CommonEvents.json syntax error: {e}")

        # Parse package.json
        package_json_path = os.path.join(project_path, "package.json")
        if os.path.exists(package_json_path):
            try:
                with open(package_json_path, "r", encoding="utf-8") as f:
                    pkg = json.load(f)
                    if isinstance(pkg, dict):
                        if not result["game_title"]:
                            result["game_title"] = pkg.get("name", "")
                        win = pkg.get("window", {})
                        if "width" in win:
                            result["screen_width"] = int(win["width"])
                        if "height" in win:
                            result["screen_height"] = int(win["height"])
            except Exception:
                pass

        # Parse js/plugins.js strictly
        plugins_js_path = os.path.join(project_path, "js", "plugins.js")
        if os.path.exists(plugins_js_path):
            try:
                with open(plugins_js_path, "r", encoding="utf-8") as f:
                    content = f.read()
                    start = content.find('[')
                    end = content.rfind(']')
                    if start != -1 and end != -1 and end > start:
                        plugin_arr = json.loads(content[start:end+1])
                        if isinstance(plugin_arr, list):
                            for p in plugin_arr:
                                if isinstance(p, dict) and "name" in p:
                                    result["plugins"].append({
                                        "name": p["name"],
                                        "status": bool(p.get("status", False)),
                                        "description": str(p.get("description", "")),
                                        "parameters": p.get("parameters", {}) if isinstance(p.get("parameters"), dict) else {}
                                    })
                            active = sum(1 for p in result["plugins"] if p["status"])
                            Logger.info(f"Detected {len(result['plugins'])} plugins declared ({active} active). Execution isolated.")
            except Exception as e:
                result["warnings"].append(f"js/plugins.js parse error: {e}")

        result["is_valid"] = (len(result["missing_files"]) == 0 and len(result["warnings"]) == 0)
        return result


def main():
    if len(sys.argv) < 2:
        print("Usage: rpg2lx_ref.py [--help|--version|inspect <path>]")
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd in ("--help", "-h"):
        print("rpg2lx - RPG Maker MV Runtime & Inspector (Reference Runner)")
        print("Usage:")
        print("  rpg2lx --help                  Show this help message")
        print("  rpg2lx --version               Show version information")
        print("  rpg2lx inspect <project_path>  Inspect RPG Maker MV project structure")
        sys.exit(0)

    if cmd in ("--version", "-v"):
        print("rpg2lx version 0.1.0-alpha (Phase 0/1 milestone)")
        sys.exit(0)

    if cmd == "inspect":
        if len(sys.argv) < 3:
            print("Error: 'inspect' requires project path argument.")
            sys.exit(1)
        res = ProjectInspector.inspect(sys.argv[2])
        print("\n--- Project Inspection Summary ---")
        print(f"Status: {'VALID RPG Maker MV Project Structure' if res['is_valid'] else 'INVALID / INCOMPLETE'}")
        print(f"Game Title: {res['game_title'] or '(Unknown)'}")
        print(f"Start Map ID: {res['start_map_id'] or '(None)'}")
        print(f"Screen Resolution: {res['screen_width']}x{res['screen_height']}")
        print(f"Database Records: {res['actor_count']} actors, {res['map_count']} maps, {res['common_event_count']} common events")
        print(f"Detected Plugins: {len(res['plugins'])}")
        for p in res['plugins']:
            status_str = "ENABLED" if p["status"] else "DISABLED"
            print(f"  - {p['name']} [{status_str}] - {p['description']}")
            for k, v in p["parameters"].items():
                print(f"      * {k} = {v}")
        if res["missing_files"]:
            print(f"Missing Files ({len(res['missing_files'])}):")
            for f in res["missing_files"]:
                print(f"  [!] {f}")
        if res["warnings"]:
            print(f"Warnings / Errors ({len(res['warnings'])}):")
            for w in res["warnings"]:
                print(f"  [*] {w}")
        sys.exit(0 if res["is_valid"] else 2)

    print(f"Unknown command: {cmd}")
    sys.exit(1)


if __name__ == "__main__":
    main()
