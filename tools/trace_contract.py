#!/usr/bin/env python3
"""Validate semantic invariants in Oracle routine trace JSON documents.

JSON Schema describes the wire format. This validator adds cross-field rules
that are awkward to express there: exact baseline ROM hashes, unique campaign
observations, matching routine locations, and policy-hook requirements.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ROM_SHA256 = {
    "ages": "0b56b78a9e45452e98c33edd111234931f1e034dc097f6f23082eb8db6055474",
    "seasons": "862a51368fb30539279d336b3fe193b43876d2cb15c87a36f5da517804ab3971",
}
RELATIONS = {"identical", "campaign-policy", "single-campaign"}
CAPTURE_STATES = {"planned", "captured", "verified"}


def validate_trace(document: dict) -> list[str]:
    errors: list[str] = []
    for field in (
        "schema_version", "case_id", "routine",
        "expected_relation", "observations",
    ):
        if field not in document:
            errors.append(f"missing required field: {field}")
    if errors:
        return errors

    if document["schema_version"] != 1:
        errors.append("schema_version must be 1")
    relation = document["expected_relation"]
    if relation not in RELATIONS:
        errors.append(f"unsupported expected_relation: {relation!r}")
    if relation == "campaign-policy" and not document.get("policy_hook"):
        errors.append("campaign-policy cases require policy_hook")

    routine = document["routine"]
    if not isinstance(routine, dict):
        return errors + ["routine must be an object"]
    locations = routine.get("locations", {})
    observations = document["observations"]
    if not isinstance(observations, list) or not observations:
        return errors + ["observations must be a non-empty array"]
    if len(observations) > 2:
        errors.append("at most two campaign observations are allowed")

    seen_campaigns = set()
    for index, observation in enumerate(observations):
        prefix = f"observations[{index}]"
        if not isinstance(observation, dict):
            errors.append(f"{prefix} must be an object")
            continue
        campaign = observation.get("campaign")
        if campaign not in ROM_SHA256:
            errors.append(f"{prefix}.campaign is invalid")
            continue
        if campaign in seen_campaigns:
            errors.append(f"duplicate observation for campaign {campaign}")
        seen_campaigns.add(campaign)
        if observation.get("rom_sha256") != ROM_SHA256[campaign]:
            errors.append(f"{prefix}.rom_sha256 is not the exact US baseline")
        expected_location = locations.get(campaign)
        if observation.get("routine_location") != expected_location:
            errors.append(
                f"{prefix}.routine_location does not match routine.locations"
            )
        status = observation.get("capture_status")
        if status not in CAPTURE_STATES:
            errors.append(f"{prefix}.capture_status is invalid")
        if status in {"captured", "verified"}:
            if "entry" not in observation:
                errors.append(f"{prefix}.entry is required for {status}")
            if "result" not in observation:
                errors.append(f"{prefix}.result is required for {status}")

    if relation in {"identical", "campaign-policy"} and seen_campaigns != {
        "ages", "seasons"
    }:
        errors.append(f"{relation} cases require Ages and Seasons observations")
    if relation == "single-campaign" and len(seen_campaigns) != 1:
        errors.append("single-campaign cases require exactly one observation")
    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    failed = False
    for path in args.paths:
        document = json.loads(path.read_text(encoding="utf-8"))
        errors = validate_trace(document)
        if errors:
            failed = True
            print(f"{path}: invalid")
            for error in errors:
                print(f"  - {error}")
        else:
            print(f"{path}: OK")
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
