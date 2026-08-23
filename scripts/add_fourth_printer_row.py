#!/usr/bin/env python3
"""SCR_PRINTER_SELECT only has 3 rows (select_printer_1/2/3), but
printerEntries in UiBridge.cpp is a std::array<PrinterUiEntry, 4> -- the
firmware supports 4 printers but the screen could only ever pick 3
(Nutzer-Report 2026-08-23).

Shrinks the 3 existing rows from height 56 (top 78/137/196, i.e. spacing 59)
to height 42 (top 78/124/170, spacing 46) and adds a 4th row select_printer_4
at top=216 (end 258), leaving a 10px gap before the bottom action bar at
top=268 -- same margin the original layout had between row 3 (end 252) and
the bar (268).

Usage:
    python scripts/add_fourth_printer_row.py                # dry-run (default)
    python scripts/add_fourth_printer_row.py --apply
"""

from __future__ import annotations

import argparse
import copy
import json
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

PROJECT_FILE = Path(__file__).resolve().parent.parent / "ui-project" / "FilamentStation.eez-project"

ROW_HEIGHT = 42
ROW_SPACING = 46
ROW_TOP_START = 78
ROW_IDENTIFIERS = ["select_printer_1", "select_printer_2", "select_printer_3", "select_printer_4"]


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
    """Returns the "children" list that directly contains the widget with
    the given identifier (so a new sibling row can be appended there)."""
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
    """Recursively replaces every "objID" with a fresh UUID (used when
    deep-copying an existing widget subtree)."""
    if isinstance(node, dict):
        if "objID" in node:
            node["objID"] = new_obj_id()
        for value in node.values():
            assign_new_obj_ids(value)
    elif isinstance(node, list):
        for item in node:
            assign_new_obj_ids(item)


def build_fourth_row(template_row: dict) -> dict:
    row = copy.deepcopy(template_row)
    assign_new_obj_ids(row)
    row["identifier"] = "select_printer_4"
    row["children"][0]["text"] = "+ freier Druckerplatz"
    return row


def resize_rows(data: dict) -> tuple[list[str], list[str]]:
    resized, missing = [], []
    for index, identifier in enumerate(ROW_IDENTIFIERS[:3]):
        widget = find_widget(data, identifier)
        if widget is None:
            missing.append(identifier)
            continue
        widget["top"] = ROW_TOP_START + index * ROW_SPACING
        widget["height"] = ROW_HEIGHT
        resized.append(identifier)
    return resized, missing


def add_fourth_row(data: dict) -> str | None:
    if find_widget(data, "select_printer_4") is not None:
        return None  # already applied
    template = find_widget(data, "select_printer_3")
    if template is None:
        raise ValueError("select_printer_3 not found -- cannot clone a 4th row")
    parent_list = find_parent_list(data, "select_printer_3")
    if parent_list is None:
        raise ValueError("could not locate select_printer_3's parent children list")
    row = build_fourth_row(template)
    row["top"] = ROW_TOP_START + 3 * ROW_SPACING
    row["height"] = ROW_HEIGHT
    insert_index = parent_list.index(template) + 1
    parent_list.insert(insert_index, row)
    return row["identifier"]


def load_project(path: Path) -> tuple[dict, str]:
    raw = path.read_text(encoding="utf-8")
    return json.loads(raw), raw


def dump_project(data: dict, path: Path) -> None:
    text = json.dumps(data, indent=2, ensure_ascii=False)
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Write changes (default is dry-run).")
    parser.add_argument("--project", type=Path, default=PROJECT_FILE)
    args = parser.parse_args()

    data, original_text = load_project(args.project)

    resized, missing = resize_rows(data)
    added = add_fourth_row(data)

    print(f"Resized: {resized}")
    if missing:
        print("Not found:", missing)
    print(f"4th row added: {added!r}" if added else "4th row: already present, skipped")

    if not resized and not added:
        print("Nothing changed, exiting without writing.")
        return 1 if missing else 0

    if not args.apply:
        print("\nDry-run only (no file written). Re-run with --apply to write.")
        return 0

    backup_path = args.project.with_name(
        f"{args.project.name}.bak-{datetime.now():%Y%m%d-%H%M%S}"
    )
    backup_path.write_text(original_text, encoding="utf-8")
    print(f"Backup written: {backup_path}")

    dump_project(data, args.project)
    print(f"Wrote: {args.project}")
    print("Next: open in EEZ Studio to confirm it loads cleanly, then re-export.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
