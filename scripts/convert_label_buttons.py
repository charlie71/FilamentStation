#!/usr/bin/env python3
"""Converts EEZ Studio LVGLLabelWidget objects that are used as buttons
(via UiBridge.cpp's bindClick()/styleLabelButton() runtime hack) into real
LVGLButtonWidget objects with a proper child label.

Background: FilamentStation.eez-project currently has 124 widgets typed as
plain LVGLLabelWidget that C++ (UiBridge.cpp::styleLabelButton()) turns into
button-looking, clickable controls at runtime by injecting a caption child
label and overriding colors/position on every update. The goal (see
TASKS.md) is for EEZ Studio to be the sole owner of screen layout -- this
script performs the one-time structural migration in the project file
itself.

Usage:
    python scripts/convert_label_buttons.py                            # dry-run (default), no changes written
    python scripts/convert_label_buttons.py --only tray_action_untag,staging_action_clear  # pilot a subset (dry-run)
    python scripts/convert_label_buttons.py --only tray_action_untag --apply  # pilot a subset, write it
    python scripts/convert_label_buttons.py --apply                    # convert all 124, write file

A timestamped backup of the project file is written before any --apply run.
The .eez-project file is native JSON (see docs/AGENTS or ui-project/); this
script edits that JSON directly. It does NOT touch the generated screens.c/
screens.h/ui.c -- those are only produced by re-exporting from EEZ Studio.
After running with --apply, open the project in EEZ Studio once to confirm
it loads cleanly and re-export before building the firmware.
"""

from __future__ import annotations

import argparse
import copy
import json
import shutil
import sys
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

PROJECT_FILE = Path(__file__).resolve().parent.parent / "ui-project" / "FilamentStation.eez-project"

# Every LVGLLabelWidget identifier that UiBridge.cpp visually turns into a
# button via styleLabelButton() (checked directly and via every array that
# gets looped through styleLabelButton()), cross-referenced against its
# "type" in the generated screens.c to keep only genuine LVGLLabelWidget
# targets (2026-08-23).
#
# Deliberately EXCLUDES 9 objects that ARE clickable (bindClick()) but never
# get styleLabelButton()'d -- e.g. tag_action_header, select_bottom_status:
# these are plain tappable text/status regions (a legitimate, distinct
# LVGL/EEZ pattern), not labels pretending to be buttons, so they stay
# LVGLLabelWidget.
LABEL_BUTTON_IDENTIFIERS: list[str] = [
    "bambu_spool_type_back",
    "bambu_spool_type_high",
    "bambu_spool_type_low",
    "bambu_spool_type_manual",
    "device_settings_back",
    "device_settings_header",
    "device_settings_restart",
    "device_settings_settings",
    "diagnostics_settings_back",
    "diagnostics_settings_header",
    "diagnostics_settings_refresh",
    "diagnostics_settings_settings",
    "firmware_settings_back",
    "firmware_settings_check",
    "firmware_settings_header",
    "firmware_settings_settings",
    "printer_edit_access_code",
    "printer_edit_cancel",
    "printer_edit_delete",
    "printer_edit_header",
    "printer_edit_host",
    "printer_edit_mask",
    "printer_edit_name",
    "printer_edit_save",
    "printer_edit_serial",
    "printer_edit_settings",
    "printer_edit_test",
    "printer_settings_active",
    "printer_settings_add",
    "printer_settings_back",
    "printer_settings_default",
    "printer_settings_edit",
    "printer_settings_enabled",
    "printer_settings_header",
    "printer_settings_row_1",
    "printer_settings_row_2",
    "printer_settings_row_3",
    "printer_settings_row_4",
    "printer_settings_settings",
    "scale_settings_back",
    "scale_settings_calibrate",
    "scale_settings_header",
    "scale_settings_reset",
    "scale_settings_settings",
    "scale_settings_tare",
    "spoolman_setting_base_path",
    "spoolman_setting_cancel",
    "spoolman_setting_host",
    "spoolman_setting_name",
    "spoolman_setting_port",
    "spoolman_setting_protocol",
    "spoolman_setting_save",
    "spoolman_setting_test",
    "spoolman_setting_timeout",
    "spoolman_settings_header",
    "spoolman_settings_settings",
    "staging_action_advanced_weight",
    "staging_action_clear",
    "staging_action_configure",
    "staging_action_erase_tag",
    "staging_action_link_tag",
    "staging_action_unlink_tag",
    "staging_actions_back",
    "staging_actions_header",
    "staging_actions_settings",
    "staging_details_close",
    "staging_details_header",
    "staging_details_more",
    "staging_details_quick_weight",
    "staging_details_settings",
    "tag_action_back",
    "tag_action_erase",
    "tag_action_select_spool",
    "tag_action_use_last_spool",
    "tag_definition_import_cancel",
    "tag_definition_import_select_spool",
    "tag_definition_import_spoolman",
    "tag_legacy_close",
    "tag_legacy_erase",
    "tag_legacy_import",
    "tag_legacy_migrate",
    "tag_legacy_select_spool",
    "tag_result_advanced_weight",
    "tag_result_close",
    "tag_result_quick_weight",
    "tag_review_back",
    "tag_review_cancel",
    "tag_review_confirm",
    "tag_unknown_close",
    "tag_unknown_select_spool",
    "tag_write_cancel",
    "tray_action_from_staging",
    "tray_action_manual",
    "tray_action_reapply",
    "tray_action_refresh",
    "tray_action_reset",
    "tray_action_untag",
    "tray_actions_back",
    "tray_actions_header",
    "tray_actions_settings",
    "tray_details_close",
    "tray_details_header",
    "tray_details_more",
    "tray_details_refresh",
    "tray_details_settings",
    "tray_details_tab_slot",
    "tray_details_tab_spool",
    "tray_select_ams_1",
    "tray_select_ams_2",
    "tray_select_ams_3",
    "tray_select_ams_4",
    "tray_select_cancel",
    "tray_select_external",
    "tray_select_header",
    "tray_select_settings",
    "tray_select_slot_1",
    "tray_select_slot_2",
    "tray_select_slot_3",
    "tray_select_slot_4",
    "wifi_settings_back",
    "wifi_settings_header",
    "wifi_settings_portal",
    "wifi_settings_reset",
    "wifi_settings_settings",
]

assert len(LABEL_BUTTON_IDENTIFIERS) == 124, len(LABEL_BUTTON_IDENTIFIERS)

# Properties that only make sense on a LVGLLabelWidget; removed from the
# widget once it becomes the LVGLButtonWidget shell (the text moves to the
# new child label instead).
LABEL_ONLY_PROPERTIES = ("text", "textType", "longMode", "recolor", "useStaticText")

# The Style currently applied to every real EEZ button in the project
# (see styles.c); the per-button color role (primary/danger/neutral) that
# UiBridge.cpp::styleLabelButton() already applies at runtime is left as-is
# for now -- defining separate ButtonDanger/ButtonNeutral EEZ Styles is a
# follow-up task (see TASKS.md, Styles/Themes), out of scope for this pure
# structural Label->Button migration.
BUTTON_STYLE = "ButtonPrimary"


def new_obj_id() -> str:
    return str(uuid.uuid4())


def find_widget(node: Any, identifier: str) -> tuple[dict, list] | tuple[None, None]:
    """Depth-first search for a widget dict with the given "identifier".

    Returns (widget_dict, containing_list) so the caller can replace the
    entry in place (widgets always live inside a "children" list).
    """
    if isinstance(node, dict):
        if node.get("identifier") == identifier and "type" in node:
            return node, None  # caller already has the dict reference
        for value in node.values():
            found, _ = find_widget(value, identifier)
            if found is not None:
                return found, None
    elif isinstance(node, list):
        for item in node:
            found, _ = find_widget(item, identifier)
            if found is not None:
                return found, None
    return None, None


def build_child_label(text: str) -> dict:
    """A centered, content-sized caption label -- matches what
    UiBridge.cpp::styleLabelButton()/centerButtonLabel() currently produce
    at runtime (full-width-minus-margin, centered, word-wrapped)."""
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


def convert_widget(widget: dict) -> None:
    if widget.get("type") != "LVGLLabelWidget":
        raise ValueError(
            f"{widget.get('identifier')!r} is type {widget.get('type')!r}, "
            "expected LVGLLabelWidget -- skipping, list may be stale"
        )
    text = widget.get("text", "")
    child = build_child_label(text)
    for prop in LABEL_ONLY_PROPERTIES:
        widget.pop(prop, None)
    widget["type"] = "LVGLButtonWidget"
    widget["children"] = [child]
    widget["useStyle"] = BUTTON_STYLE


def load_project(path: Path) -> tuple[dict, str]:
    raw = path.read_text(encoding="utf-8")
    return json.loads(raw), raw


def dump_project(data: dict, path: Path) -> None:
    text = json.dumps(data, indent=2, ensure_ascii=False)
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--apply", action="store_true", help="Write changes (default is dry-run)."
    )
    parser.add_argument(
        "--only",
        metavar="ID1,ID2,...",
        help="Comma-separated subset of identifiers to convert (for piloting).",
    )
    parser.add_argument(
        "--project",
        type=Path,
        default=PROJECT_FILE,
        help=f"Path to the .eez-project file (default: {PROJECT_FILE}).",
    )
    args = parser.parse_args()

    targets = LABEL_BUTTON_IDENTIFIERS
    if args.only:
        requested = [name.strip() for name in args.only.split(",") if name.strip()]
        unknown = [name for name in requested if name not in LABEL_BUTTON_IDENTIFIERS]
        if unknown:
            print(f"error: not in the known label-button list: {unknown}", file=sys.stderr)
            return 1
        targets = requested

    data, original_text = load_project(args.project)

    converted, missing, failed = [], [], []
    for identifier in targets:
        widget, _ = find_widget(data, identifier)
        if widget is None:
            missing.append(identifier)
            continue
        try:
            convert_widget(widget)
            converted.append(identifier)
        except ValueError as exc:
            failed.append(str(exc))

    print(f"Requested: {len(targets)}  Converted: {len(converted)}  "
          f"Missing: {len(missing)}  Failed: {len(failed)}")
    if missing:
        print("Not found in project (identifier typo or already changed):")
        for name in missing:
            print(f"  - {name}")
    if failed:
        print("Skipped (unexpected current type):")
        for msg in failed:
            print(f"  - {msg}")

    if not converted:
        print("Nothing converted, exiting without writing.")
        return 1 if (missing or failed) else 0

    if not args.apply:
        print("\nDry-run only (no file written). Re-run with --apply to write, "
              "or --only <names> to pilot a subset first.")
        return 0

    backup_path = args.project.with_name(
        f"{args.project.name}.bak-{datetime.now():%Y%m%d-%H%M%S}"
    )
    backup_path.write_text(original_text, encoding="utf-8")
    print(f"Backup written: {backup_path}")

    dump_project(data, args.project)
    print(f"Wrote: {args.project}")
    print("Next: open the project in EEZ Studio to confirm it loads cleanly, "
          "then re-export (screens.c/screens.h/ui.c) before building the firmware.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
