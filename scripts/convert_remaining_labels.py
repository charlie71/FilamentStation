#!/usr/bin/env python3
"""Converts remaining LVGLLabelWidget objects that should be real
LVGLButtonWidget controls into buttons.

Round 2 (Nutzer-Report 2026-08-23): 7 tag_*_settings gear buttons
(tag_action_settings, tag_review_settings, tag_write_settings,
tag_result_settings, tag_definition_import_settings, tag_legacy_settings,
tag_unknown_settings) -- same left=412,top=0,width=68,height=40,text=""
shape as every other screen's already-converted "_settings" button (e.g.
device_settings_settings). Missed in the original 124-item migration
(scripts/convert_label_buttons.py) because they're bound via
bindClick(..., settingsClicked) directly, without ever going through
styleLabelButton(), so they didn't show up in that audit.

Round 1 (already applied 2026-08-23, kept here for history): converted
select_bottom_status (SCR_PRINTER_SELECT) the same simple way, and the 7
tag_*_header widgets via convert_header() below, which PREPENDS the new
caption child instead of replacing "children" wholesale -- those already
carried 3 status-icon LVGLImageWidget children (added by
scripts/add_header_status_icons.py while they were still labels) that a
plain convert_simple() would have silently deleted. Also deleted home_status
(SCR_HOME): its info is now redundant with the header status icons.

Usage:
    python scripts/convert_remaining_labels.py                # dry-run (default)
    python scripts/convert_remaining_labels.py --apply
"""

from __future__ import annotations

import argparse
import json
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

PROJECT_FILE = Path(__file__).resolve().parent.parent / "ui-project" / "FilamentStation.eez-project"

SIMPLE_TARGETS: list[str] = [
    "tag_action_settings",
    "tag_review_settings",
    "tag_write_settings",
    "tag_result_settings",
    "tag_definition_import_settings",
    "tag_legacy_settings",
    "tag_unknown_settings",
]
# Round 1 targets, already converted -- left empty so re-running this script
# is a no-op for them instead of re-processing (and erroring on) widgets
# that are no longer LVGLLabelWidget.
DELETE_TARGETS: list[str] = []
HEADER_TARGETS: list[str] = []

LABEL_ONLY_PROPERTIES = ("text", "textType", "longMode", "recolor", "useStaticText")
BUTTON_STYLE = "ButtonPrimary"


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


def delete_widget(node: Any, identifier: str) -> bool:
    """Removes the widget with the given identifier from whichever
    "children" list contains it. Returns True if found and removed."""
    if isinstance(node, dict):
        children = node.get("children")
        if isinstance(children, list):
            for item in children:
                if isinstance(item, dict) and item.get("identifier") == identifier and "type" in item:
                    children.remove(item)
                    return True
        for value in node.values():
            if delete_widget(value, identifier):
                return True
    elif isinstance(node, list):
        for item in node:
            if delete_widget(item, identifier):
                return True
    return False


def build_child_label(text: str) -> dict:
    return {
        "objID": new_obj_id(),
        "type": "LVGLLabelWidget",
        "left": 0,
        "top": 0,
        "width": 0,
        "height": 0,
        "customInputs": [],
        "customOutputs": [],
        "style": {
            "objID": new_obj_id(),
            "useStyle": "default",
            "conditionalStyles": [],
            "childStyles": [],
        },
        "timeline": [],
        "eventHandlers": [],
        "leftUnit": "px",
        "topUnit": "px",
        "widthUnit": "content",
        "heightUnit": "content",
        "children": [],
        "widgetFlags": "GESTURE_BUBBLE|SNAPPABLE",
        "hiddenFlagType": "literal",
        "clickableFlag": False,
        "clickableFlagType": "literal",
        "checkedStateType": "literal",
        "disabledStateType": "literal",
        "states": "",
        "localStyles": {
            "objID": new_obj_id(),
            "definition": {"MAIN": {"DEFAULT": {"align": "CENTER"}}},
        },
        "groupIndex": 0,
        "text": text,
        "textType": "literal",
        "longMode": "WRAP",
        "recolor": False,
        "useStaticText": True,
    }


def convert_simple(widget: dict) -> None:
    if widget.get("type") != "LVGLLabelWidget":
        raise ValueError(f"{widget.get('identifier')!r} is type {widget.get('type')!r}, expected LVGLLabelWidget")
    text = widget.get("text", "")
    child = build_child_label(text)
    for prop in LABEL_ONLY_PROPERTIES:
        widget.pop(prop, None)
    widget["type"] = "LVGLButtonWidget"
    widget["children"] = [child]
    widget["useStyle"] = BUTTON_STYLE


def convert_header(widget: dict) -> None:
    if widget.get("type") != "LVGLLabelWidget":
        raise ValueError(f"{widget.get('identifier')!r} is type {widget.get('type')!r}, expected LVGLLabelWidget")
    text = widget.get("text", "")
    child = build_child_label(text)
    for prop in LABEL_ONLY_PROPERTIES:
        widget.pop(prop, None)
    widget["type"] = "LVGLButtonWidget"
    widget["children"] = [child] + widget.get("children", [])
    widget["useStyle"] = BUTTON_STYLE


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

    converted, missing, failed = [], [], []
    deleted, delete_missing = [], []
    for identifier in DELETE_TARGETS:
        if delete_widget(data, identifier):
            deleted.append(identifier)
        else:
            delete_missing.append(identifier)

    for identifier in SIMPLE_TARGETS:
        widget = find_widget(data, identifier)
        if widget is None:
            missing.append(identifier)
            continue
        try:
            convert_simple(widget)
            converted.append(identifier)
        except ValueError as exc:
            failed.append(str(exc))

    for identifier in HEADER_TARGETS:
        widget = find_widget(data, identifier)
        if widget is None:
            missing.append(identifier)
            continue
        try:
            existing_child_count = len(widget.get("children", []))
            convert_header(widget)
            converted.append(f"{identifier} (+{existing_child_count} preserved icon children)")
        except ValueError as exc:
            failed.append(str(exc))

    print(f"Deleted: {len(deleted)}  Delete-missing: {len(delete_missing)}")
    for name in deleted:
        print(f"  - {name}")
    if delete_missing:
        print("Not found (delete):", delete_missing)

    print(f"Converted: {len(converted)}  Missing: {len(missing)}  Failed: {len(failed)}")
    for name in converted:
        print(f"  + {name}")
    if missing:
        print("Not found:", missing)
    if failed:
        print("Skipped:", failed)

    if not converted and not deleted:
        print("Nothing changed, exiting without writing.")
        return 1 if (missing or failed or delete_missing) else 0

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
