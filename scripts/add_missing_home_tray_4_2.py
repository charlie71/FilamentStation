#!/usr/bin/env python3
"""The user manually added 11 of the 12 home_<tray>_1/_2 multicolor swatch
containers to SCR_HOME in EEZ Studio (Nutzerwunsch 2026-08-23) -- only
home_tray_4_2 is missing (likely a copy-paste oversight: home_tray_4 only
got a "_1" swatch, every other button got both). Clones home_tray_4_1
(same left+41, same top/width/height/style shape) as home_tray_4_2 so the
set is complete and updateHomeContent() can rely on all 12 existing.

Usage:
    python scripts/add_missing_home_tray_4_2.py                # dry-run (default)
    python scripts/add_missing_home_tray_4_2.py --apply
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

PROJECT_FILE = Path(__file__).resolve().parent.parent / "ui-project" / "FilamentStation.eez-project"


def new_obj_id() -> str:
    return str(uuid.uuid4())


def find_widget(node: Any, identifier: str) -> dict | None:
    if isinstance(node, dict):
        if node.get("identifier") == identifier and "type" in node:
            return node
        for value in node.values():
            found = find_widget(value, identifier)
            if found is not None:
                return found
    elif isinstance(node, list):
        for item in node:
            found = find_widget(item, identifier)
            if found is not None:
                return found
    return None


def find_parent_list(node: Any, identifier: str) -> list | None:
    if isinstance(node, dict):
        children = node.get("children")
        if isinstance(children, list):
            for item in children:
                if isinstance(item, dict) and item.get("identifier") == identifier:
                    return children
        for value in node.values():
            found = find_parent_list(value, identifier)
            if found is not None:
                return found
    elif isinstance(node, list):
        for item in node:
            found = find_parent_list(item, identifier)
            if found is not None:
                return found
    return None


def assign_new_obj_ids(node: Any) -> None:
    if isinstance(node, dict):
        if "objID" in node:
            node["objID"] = new_obj_id()
        for value in node.values():
            assign_new_obj_ids(value)
    elif isinstance(node, list):
        for item in node:
            assign_new_obj_ids(item)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Write changes (default is dry-run).")
    parser.add_argument("--project", type=Path, default=PROJECT_FILE)
    args = parser.parse_args()

    raw = args.project.read_text(encoding="utf-8")
    data = json.loads(raw)

    if find_widget(data, "home_tray_4_2") is not None:
        print("home_tray_4_2 already present, nothing to do.")
        return 0

    template = find_widget(data, "home_tray_4_1")
    if template is None:
        print("error: home_tray_4_1 not found", file=sys.stderr)
        return 1
    parent_list = find_parent_list(data, "home_tray_4_1")
    if parent_list is None:
        print("error: could not locate home_tray_4_1's parent children list", file=sys.stderr)
        return 1

    clone = copy.deepcopy(template)
    assign_new_obj_ids(clone)
    clone["identifier"] = "home_tray_4_2"
    clone["left"] = int(template["left"]) + 41

    parent_list.insert(parent_list.index(template) + 1, clone)
    print(f"Added home_tray_4_2 at left={clone['left']}, top={clone['top']}")

    if not args.apply:
        print("\nDry-run only (no file written). Re-run with --apply to write.")
        return 0

    backup_path = args.project.with_name(
        f"{args.project.name}.bak-{datetime.now():%Y%m%d-%H%M%S}"
    )
    backup_path.write_text(raw, encoding="utf-8")
    print(f"Backup written: {backup_path}")

    args.project.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8", newline="\n")
    print(f"Wrote: {args.project}")
    print("Next: open in EEZ Studio to confirm it loads cleanly, then re-export.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
