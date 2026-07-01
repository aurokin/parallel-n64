#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path

from hires_pack_emit_binary_package import emit_binary_package_from_manifest
from hires_pack_emit_loader_manifest import summarize_runtime_records
from hires_pack_materialize_package import materialize_package_in_memory
from hts2phrb import make_streaming_asset_blob_loader


def load_json(path: Path):
    return json.loads(path.read_text())


def load_exclusion_review(path: Path):
    data = load_json(path)
    schema_version = int(data.get("schema_version") or 0)
    if schema_version != 1:
        raise SystemExit(f"exclusion review {path} must have schema_version=1")
    kind = str(data.get("kind") or "")
    if kind != "family-exclusion-review":
        raise SystemExit(f"exclusion review {path} must have kind='family-exclusion-review'")
    exclusions = data.get("exclusions") or []
    if not isinstance(exclusions, list) or not exclusions:
        raise SystemExit(f"exclusion review {path} key exclusions must be a non-empty list")
    for index, exclusion in enumerate(exclusions):
        if not isinstance(exclusion, dict):
            raise SystemExit(f"exclusion review {path} exclusion #{index} must be an object")
        if not exclusion.get("policy_key"):
            raise SystemExit(f"exclusion review {path} exclusion #{index} must provide policy_key")
        if not exclusion.get("reason"):
            raise SystemExit(f"exclusion review {path} exclusion #{index} must provide reason")
    return {"path": str(path.resolve()), "exclusions": exclusions}


def apply_exclusions(loader_manifest: dict, review_docs: list[dict]):
    records = loader_manifest.get("records") or []
    changes = []

    for review in review_docs:
        for exclusion in review["exclusions"]:
            policy_key = str(exclusion["policy_key"])
            sampled_low32 = str(exclusion.get("sampled_low32") or "").lower()
            matched = [
                record for record in records
                if str(record.get("policy_key") or "") == policy_key
            ]
            if not matched:
                raise SystemExit(f"exclusion policy_key={policy_key} matched no records")
            if len(matched) > 1:
                raise SystemExit(f"exclusion policy_key={policy_key} matched {len(matched)} records")
            record = matched[0]
            record_low32 = str(
                (record.get("canonical_identity") or {}).get("sampled_low32") or ""
            ).lower()
            if sampled_low32 and record_low32 != sampled_low32:
                raise SystemExit(
                    f"exclusion policy_key={policy_key} sampled_low32 mismatch: "
                    f"review={sampled_low32} record={record_low32}"
                )
            records.remove(record)
            changes.append(
                {
                    "policy_key": policy_key,
                    "sampled_low32": record_low32,
                    "record_kind": record.get("record_kind"),
                    "removed_replacement_ids": [
                        candidate.get("replacement_id")
                        for candidate in (record.get("asset_candidates") or [])
                    ],
                    "reason": exclusion["reason"],
                    "evidence": exclusion.get("evidence"),
                    "review_source_path": review["path"],
                }
            )

    loader_manifest["records"] = records
    loader_manifest.update(summarize_runtime_records(records))
    applied = loader_manifest.get("applied_exclusion_reviews") or []
    applied.extend(review["path"] for review in review_docs)
    loader_manifest["applied_exclusion_reviews"] = applied
    return loader_manifest, changes


def main():
    parser = argparse.ArgumentParser(
        description="Apply family exclusion review decisions to a package loader manifest."
    )
    parser.add_argument("--loader-manifest", required=True, help="Source loader-manifest.json")
    parser.add_argument(
        "--exclusion-review", action="append", required=True,
        help="One or more family exclusion review JSON files",
    )
    parser.add_argument("--output-dir", required=True, help="Output directory for candidate loader/package artifacts")
    parser.add_argument("--package-name", default="package.phrb", help="Binary package filename relative to output-dir")
    args = parser.parse_args()

    loader_manifest_path = Path(args.loader_manifest)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    loader_manifest = load_json(loader_manifest_path)
    review_docs = [load_exclusion_review(Path(path)) for path in args.exclusion_review]
    updated_manifest, changes = apply_exclusions(loader_manifest, review_docs)

    output_loader_manifest = output_dir / "loader-manifest.json"
    output_loader_manifest.write_text(json.dumps(updated_manifest, indent=2) + "\n")

    # Mirror the hts2phrb packaging stage: no PNG previews, legacy-mode blobs
    # streamed from the source cache, so the curated package matches the
    # canonical package format byte-conventions.
    package_dir = output_dir / "package"
    package_manifest, asset_rgba_blobs = materialize_package_in_memory(
        output_loader_manifest,
        package_dir,
        emit_png_assets=False,
        include_asset_blobs=False,
        compute_review_hashes=False,
    )
    package_dir.mkdir(parents=True, exist_ok=True)
    (package_dir / "package-manifest.json").write_text(json.dumps(package_manifest, indent=2) + "\n")
    binary_path = output_dir / args.package_name
    streaming_asset_blob_loader = make_streaming_asset_blob_loader(
        package_manifest, asset_storage_mode="legacy"
    )
    binary_result = emit_binary_package_from_manifest(
        package_manifest,
        binary_path,
        asset_rgba_blobs,
        asset_blob_loader=streaming_asset_blob_loader,
        asset_storage_mode="legacy",
    )

    result = {
        "source_loader_manifest_path": str(loader_manifest_path),
        "exclusion_review_paths": args.exclusion_review,
        "output_loader_manifest_path": str(output_loader_manifest),
        "package_dir": str(package_dir),
        "package_manifest_record_count": package_manifest.get("record_count"),
        "applied_exclusions": changes,
        "binary_package": binary_result,
    }
    sys.stdout.write(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
