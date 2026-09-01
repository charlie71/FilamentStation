#!/usr/bin/env python3
"""Adds three status-icon LVGLImageWidget children (printer/WiFi/Spoolman
connection state) to every header widget in FilamentStation.eez-project, so
the header bar is unified across all screens (Nutzerwunsch 2026-08-23):

    [3D-Printer-Icon] [Druckername] [Drucker-Status-Icon] ... [WLAN-Icon] [Spoolman-Icon]

Each new icon is a single LVGLImageWidget (not a connected/disconnected
pair) -- the "connected" image is only the design-time placeholder; C++
(UiBridge.cpp) swaps the actual lv_img_dsc_t source at runtime via
lv_image_set_src() based on real state, so only 3 new objects per header
are needed (69 total across 23 headers), not 6.

Usage:
    python scripts/add_header_status_icons.py                # dry-run (default)
    python scripts/add_header_status_icons.py --only home_header,select_header
    python scripts/add_header_status_icons.py --apply         # write all 23
"""

from __future__ import annotations

import argparse
import json
import sys
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

PROJECT_FILE = Path(__file__).resolve().parent.parent / "ui-project" / "FilamentStation.eez-project"

# Every header widget identifier (see UiBridge.cpp::setAllHeaderTexts()).
HEADER_IDENTIFIERS: list[str] = [
    "home_header",
    "select_header",
    "settings_header",
    "staging_details_header",
    "staging_actions_header",
    "tray_details_header",
    "tray_actions_header",
    "tray_select_header",
    "spoolman_settings_header",
    "printer_settings_header",
    "printer_edit_header",
    "wifi_settings_header",
    "scale_settings_header",
    "device_settings_header",
    "diagnostics_settings_header",
    "firmware_settings_header",
    "tag_action_header",
    "tag_review_header",
    "tag_write_header",
    "tag_result_header",
    "tag_definition_import_header",
    "tag_legacy_header",
    "tag_unknown_header",
]

assert len(HEADER_IDENTIFIERS) == 23, len(HEADER_IDENTIFIERS)

# (identifier suffix, EEZ image display name (see images.c's `images[]`
# table -- exact spelling/casing, including the project's own "conneced"
# typo), natural pixel size (w, h)). Order matches the requested layout,
# right-to-left placement below.
ICON_SLOT_WIDTH = 32
ICON_GAP = 8
RIGHT_MARGIN = 8
HEADER_HEIGHT = 40

ICONS: list[tuple[str, str, tuple[int, int]]] = [
    ("printer", "conneced W", (24, 24)),
    ("wifi", "WIFI connected W", (32, 32)),
    ("spoolman", "Spoolman connected W", (32, 32)),
]


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


def build_icon_widget(identifier: str, image_name: str, left: int, top: int,
                      width: int, height: int) -> dict:
    return {
        "objID": new_obj_id(),
        "type": "LVGLImageWidget",
        "left": left,
        "top": top,
        "width": width,
        "height": height,
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
        "identifier": identifier,
        "leftUnit": "px",
        "topUnit": "px",
        "widthUnit": "content",
        "heightUnit": "content",
        "children": [],
        "widgetFlags": (
            "CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLLABLE|"
            "SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|"
            "SNAPPABLE"
        ),
        "hiddenFlagType": "literal",
        "clickableFlag": False,
        "clickableFlagType": "literal",
        "flagScrollbarMode": "",
        "flagScrollDirection": "",
        "scrollSnapX": "",
        "scrollSnapY": "",
        "checkedStateType": "literal",
        "disabledStateType": "literal",
        "states": "",
        "localStyles": {"objID": new_obj_id()},
        "group": "",
        "groupIndex": 0,
        "image": image_name,
        "setPivot": False,
        "pivotX": 0,
        "pivotY": 0,
        "zoom": 256,
        "angle": 0,
        "innerAlign": "CENTER",
        "sizeMode": "VIRTUAL",
        "value": 0,
        "valueType": "literal",
        "previewValue": 0,
    }


def add_icons_to_header(header: dict) -> list[str]:
    # Most headers are LVGLButtonWidget (see scripts/convert_label_buttons.py),
    # but the 7 tag_*_header screens are still plain LVGLLabelWidget (they
    # were deliberately excluded from that conversion -- clickable status
    # text, not a button). Both widget types support "children"/"width" in
    # the EEZ schema, so appending icon children works for either.
    if header.get("type") not in ("LVGLButtonWidget", "LVGLLabelWidget"):
        raise ValueError(
            f"{header.get('identifier')!r} is type {header.get('type')!r}, "
            "expected LVGLButtonWidget or LVGLLabelWidget"
        )
    header_width = int(header["width"])
    added = []
    # Rightmost icon first (spoolman), moving left (wifi, then printer) --
    # matches the requested left-to-right reading order after the text:
    # [status][wifi][spoolman].
    right_edge = header_width - RIGHT_MARGIN
    for suffix, image_name, (icon_w, icon_h) in reversed(ICONS):
        slot_left = right_edge - ICON_SLOT_WIDTH
        left = slot_left + (ICON_SLOT_WIDTH - icon_w) // 2
        top = (HEADER_HEIGHT - icon_h) // 2
        identifier = f"{header['identifier']}_{suffix}"
        widget = build_icon_widget(identifier, image_name, left, top, icon_w, icon_h)
        header["children"].append(widget)
        added.append(identifier)
        right_edge = slot_left - ICON_GAP
    return added


def load_project(path: Path) -> tuple[dict, str]:
    raw = path.read_text(encoding="utf-8")
    return json.loads(raw), raw


def dump_project(data: dict, path: Path) -> None:
    text = json.dumps(data, indent=2, ensure_ascii=False)
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Write changes (default is dry-run).")
    parser.add_argument("--only", metavar="ID1,ID2,...", help="Comma-separated subset of header identifiers.")
    parser.add_argument("--project", type=Path, default=PROJECT_FILE)
    args = parser.parse_args()

    targets = HEADER_IDENTIFIERS
    if args.only:
        requested = [name.strip() for name in args.only.split(",") if name.strip()]
        unknown = [name for name in requested if name not in HEADER_IDENTIFIERS]
        if unknown:
            print(f"error: not a known header: {unknown}", file=sys.stderr)
            return 1
        targets = requested

    data, original_text = load_project(args.project)

    processed, missing, failed = [], [], []
    for identifier in targets:
        header = find_widget(data, identifier)
        if header is None:
            missing.append(identifier)
            continue
        try:
            added = add_icons_to_header(header)
            processed.append((identifier, added))
        except ValueError as exc:
            failed.append(str(exc))

    print(f"Requested: {len(targets)}  Processed: {len(processed)}  "
          f"Missing: {len(missing)}  Failed: {len(failed)}")
    for identifier, added in processed:
        print(f"  {identifier}: +{added}")
    if missing:
        print("Not found:", missing)
    if failed:
        print("Skipped:", failed)

    if not processed:
        print("Nothing to write.")
        return 1 if (missing or failed) else 0

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
