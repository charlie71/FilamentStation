#!/usr/bin/env python3
"""Adds "ButtonNeutral" and "ButtonDanger" LVGL styles to
FilamentStation.eez-project, referencing two new named theme colors, and
assigns them to every button that UiBridge.cpp currently colors at runtime
via styleLabelButton(object, kColorNeutralGrey/kColorDangerRed) (Nutzerwunsch
2026-08-23: einheitliches Aussehen ueber EEZ-Styles statt C++-Laufzeit-
Ueberschreibung).

"ButtonPrimary" (the existing style, #2196f3 background / white text) stays
the default for every other button -- already applied project-wide by
scripts/convert_label_buttons.py.

Usage:
    python scripts/add_button_role_styles.py                # dry-run (default)
    python scripts/add_button_role_styles.py --apply
"""

from __future__ import annotations

import argparse
import json
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

PROJECT_FILE = Path(__file__).resolve().parent.parent / "ui-project" / "FilamentStation.eez-project"

# (style name, color name, hex value) -- hex matches UiBridge.cpp's
# kColorNeutralGrey/kColorDangerRed exactly, so the EEZ theme and the (now
# vestigial) C++ palette agree until the C++ side is simplified to stop
# passing a color at all.
NEW_COLOR_STYLES: list[tuple[str, str, str]] = [
    ("ButtonNeutral", "ButtonNeutralActive", "#455a64"),
    ("ButtonDanger", "ButtonDangerActive", "#c62828"),
]

# Every identifier styleLabelButton() currently colors kColorNeutralGrey
# (2026-08-23 audit, see TASKS.md).
NEUTRAL_BUTTONS: list[str] = [
    "bambu_spool_type_back",
    "device_settings_back",
    "diagnostics_settings_back",
    "firmware_settings_back",
    "printer_edit_cancel",
    "printer_settings_back",
    "scale_settings_back",
    "spoolman_setting_cancel",
    "staging_actions_back",
    "tag_definition_import_cancel",
    "tag_review_cancel",
    "tag_write_cancel",
    "tray_actions_back",
    "tray_select_cancel",
    "wifi_settings_back",
]

# Every identifier styleLabelButton() currently colors kColorDangerRed.
DANGER_BUTTONS: list[str] = [
    "printer_edit_delete",
    "staging_action_clear",
    "tag_action_erase",
    "tag_legacy_erase",
    "tray_action_reset",
    "tray_action_untag",
]

assert len(NEUTRAL_BUTTONS) == 15, len(NEUTRAL_BUTTONS)
assert len(DANGER_BUTTONS) == 6, len(DANGER_BUTTONS)
# home_active_ams/home_ams_4 are deliberately excluded: their color is fully
# runtime/data-driven (updateHomeContent(), per-AMS colored border), the
# static styleLabelButton(..., kColorNeutralGrey) call on them only sets an
# initial placeholder that's overwritten every refresh -- leaving their
# useStyle at ButtonPrimary is harmless and correct.


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


def ensure_colors_and_theme(data: dict) -> dict[str, int]:
    """Appends any missing named colors + their theme values; returns a
    name->index map for every color referenced by NEW_COLOR_STYLES (both
    pre-existing and newly added)."""
    colors: list[dict] = data["colors"]
    existing_names = {c["name"]: i for i, c in enumerate(colors)}
    theme = data["themes"][0]
    assert theme["name"] == "Default", theme["name"]

    for _, color_name, hex_value in NEW_COLOR_STYLES:
        if color_name in existing_names:
            continue
        colors.append({"objID": new_obj_id(), "name": color_name})
        theme["colors"].append(hex_value)
        existing_names[color_name] = len(colors) - 1

    assert len(colors) == len(theme["colors"]), (
        f"colors/theme length mismatch: {len(colors)} vs {len(theme['colors'])}"
    )
    return existing_names


def ensure_styles(data: dict) -> None:
    styles: list[dict] = data["lvglStyles"]["styles"]
    existing_style_names = {s["name"] for s in styles}
    for style_name, color_name, _ in NEW_COLOR_STYLES:
        if style_name in existing_style_names:
            continue
        styles.append({
            "objID": new_obj_id(),
            "name": style_name,
            "forWidgetType": "LVGLButtonWidget",
            "childStyles": [],
            "definition": {
                "objID": new_obj_id(),
                "definition": {
                    "MAIN": {
                        "DEFAULT": {
                            "bg_color": color_name,
                            "text_color": "ButtonTextActive",
                        },
                        "DISABLED": {
                            "bg_color": "ButtonDisabled",
                            "text_color": "ButtonTextDisabled",
                        },
                    }
                },
            },
        })


def apply_style_to_buttons(data: dict, identifiers: list[str], style_name: str) -> tuple[list[str], list[str]]:
    updated, missing = [], []
    for identifier in identifiers:
        widget = find_widget(data, identifier)
        if widget is None:
            missing.append(identifier)
            continue
        if widget.get("type") != "LVGLButtonWidget":
            missing.append(f"{identifier} (type={widget.get('type')})")
            continue
        widget["useStyle"] = style_name
        updated.append(identifier)
    return updated, missing


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

    color_indices = ensure_colors_and_theme(data)
    print("Color indices:", color_indices)
    ensure_styles(data)
    print("Styles now defined:", [s["name"] for s in data["lvglStyles"]["styles"]])

    neutral_updated, neutral_missing = apply_style_to_buttons(data, NEUTRAL_BUTTONS, "ButtonNeutral")
    danger_updated, danger_missing = apply_style_to_buttons(data, DANGER_BUTTONS, "ButtonDanger")

    print(f"ButtonNeutral applied: {len(neutral_updated)}/{len(NEUTRAL_BUTTONS)}"
          f"{' MISSING: ' + str(neutral_missing) if neutral_missing else ''}")
    print(f"ButtonDanger applied: {len(danger_updated)}/{len(DANGER_BUTTONS)}"
          f"{' MISSING: ' + str(danger_missing) if danger_missing else ''}")

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
