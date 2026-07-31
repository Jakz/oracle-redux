#!/usr/bin/env python3
"""Validate and summarize the Oracle Redux broad-spectrum slice matrix."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT / "specs" / "slices.json"
SCENARIOS = {
    "latest", "explore", "chest", "vasu", "octorok", "hole", "water", "atlas"
}
CAMPAIGNS = {"ages", "seasons"}


def load_catalog(path: Path = DEFAULT_CATALOG) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_catalog(catalog: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if catalog.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    statuses = set(catalog.get("statuses", []))
    priorities = set(catalog.get("priorities", []))
    tracks = set(catalog.get("tracks", []))
    slices = catalog.get("slices", [])
    ids = [entry.get("id") for entry in slices]
    known_ids = set(ids)
    duplicate_ids = sorted(name for name, count in Counter(ids).items() if count > 1)
    if duplicate_ids:
        errors.append(f"duplicate slice ids: {', '.join(duplicate_ids)}")

    required = {
        "id",
        "track",
        "title",
        "status",
        "priority",
        "playability",
        "campaigns",
        "scenarios",
        "evidence",
        "verification",
        "depends_on",
        "gaps",
    }
    for index, entry in enumerate(slices):
        label = entry.get("id", f"entry #{index}")
        missing = sorted(required - set(entry))
        if missing:
            errors.append(f"{label}: missing fields {', '.join(missing)}")
            continue
        if entry["track"] not in tracks:
            errors.append(f"{label}: unknown track {entry['track']}")
        if entry["status"] not in statuses:
            errors.append(f"{label}: unknown status {entry['status']}")
        if entry["priority"] not in priorities:
            errors.append(f"{label}: unknown priority {entry['priority']}")
        unknown_campaigns = set(entry["campaigns"]) - CAMPAIGNS
        if unknown_campaigns:
            errors.append(f"{label}: unknown campaigns {sorted(unknown_campaigns)}")
        if set(entry["campaigns"]) != CAMPAIGNS:
            errors.append(f"{label}: both campaigns must be tracked explicitly")
        unknown_scenarios = set(entry["scenarios"]) - SCENARIOS
        if unknown_scenarios:
            errors.append(f"{label}: unknown scenarios {sorted(unknown_scenarios)}")
        unknown_dependencies = set(entry["depends_on"]) - known_ids
        if unknown_dependencies:
            errors.append(f"{label}: unknown dependencies {sorted(unknown_dependencies)}")
        for field in ("evidence", "verification"):
            for relative in entry[field]:
                if not (ROOT / relative).is_file():
                    errors.append(f"{label}: missing {field} file {relative}")
        if entry["status"] == "verified" and not entry["verification"]:
            errors.append(f"{label}: verified slices require verification evidence")

    covered_tracks = {entry.get("track") for entry in slices}
    missing_tracks = sorted(tracks - covered_tracks)
    if missing_tracks:
        errors.append(f"tracks without slices: {', '.join(missing_tracks)}")
    return errors


def print_summary(catalog: dict[str, Any]) -> None:
    slices = catalog["slices"]
    by_status = Counter(entry["status"] for entry in slices)
    by_track: dict[str, Counter[str]] = {
        track: Counter(
            entry["status"] for entry in slices if entry["track"] == track
        )
        for track in catalog["tracks"]
    }
    print(f"Oracle Redux slice matrix: {len(slices)} slices")
    print("Status: " + ", ".join(
        f"{status}={by_status[status]}" for status in catalog["statuses"]
    ))
    print("\nTracks:")
    for track, counts in by_track.items():
        details = ", ".join(
            f"{status}={counts[status]}"
            for status in catalog["statuses"]
            if counts[status]
        )
        print(f"  {track:18} {details}")
    print("\nNow:")
    for entry in slices:
        if entry["priority"] == "now" and entry["status"] != "verified":
            print(f"  {entry['id']}: {entry['title']} [{entry['status']}]")
    print("\nNext:")
    for entry in slices:
        if entry["priority"] == "next":
            print(f"  {entry['id']}: {entry['title']} [{entry['status']}]")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument(
        "--check",
        action="store_true",
        help="validate only and remain quiet on success",
    )
    args = parser.parse_args()
    catalog = load_catalog(args.catalog)
    errors = validate_catalog(catalog)
    if errors:
        for error in errors:
            print(f"slice catalog error: {error}")
        return 1
    if not args.check:
        print_summary(catalog)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
