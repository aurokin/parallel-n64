#!/usr/bin/env python3
import argparse
import json
import math
import sys
from pathlib import Path

from PIL import Image


DEFAULT_ORACLE = Path(
    "/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/"
    "scaling/oracle-gliden64-4x-hires-off-2/Paper Mario (USA)-260306-212123.png"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare a Paper Mario parallel scaling capture against the saved GLide oracle."
    )
    parser.add_argument("--candidate", required=True, help="Candidate PNG to compare.")
    parser.add_argument("--oracle", default=str(DEFAULT_ORACLE), help="Oracle PNG to compare against.")
    parser.add_argument("--output-dir", required=True, help="Directory to write metrics and visual diffs into.")
    return parser.parse_args()


def load_rgb(path: Path) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(f"missing image: {path}")
    return Image.open(path).convert("RGB")


def grayscale_array(image: Image.Image):
    import numpy as np

    return np.asarray(image.convert("L"), dtype=np.float32)


def rgb_array(image: Image.Image):
    import numpy as np

    return np.asarray(image, dtype=np.float32)


def find_best_offset(oracle: Image.Image, candidate: Image.Image) -> tuple[int, int, float]:
    import numpy as np

    oracle_gray = grayscale_array(oracle)
    candidate_gray = grayscale_array(candidate)
    oracle_h, oracle_w = oracle_gray.shape
    candidate_h, candidate_w = candidate_gray.shape
    if oracle_w < candidate_w or oracle_h < candidate_h:
        raise ValueError("oracle must be at least as large as candidate")

    margin_x = min(200, max(0, candidate_w // 8))
    margin_y = min(200, max(0, candidate_h // 8))
    sample = candidate_gray[margin_y:candidate_h - margin_y:6, margin_x:candidate_w - margin_x:6]

    def evaluate(dx: int, dy: int) -> float:
        window = oracle_gray[
            dy + margin_y:dy + candidate_h - margin_y:6,
            dx + margin_x:dx + candidate_w - margin_x:6,
        ]
        return float(np.mean(np.abs(window - sample)))

    max_dx = oracle_w - candidate_w
    max_dy = oracle_h - candidate_h
    best = (float("inf"), 0, 0)

    for dy in range(0, max_dy + 1, 4):
        for dx in range(0, max_dx + 1, 4):
            score = evaluate(dx, dy)
            if score < best[0]:
                best = (score, dx, dy)

    coarse_dx = best[1]
    coarse_dy = best[2]
    for dy in range(max(0, coarse_dy - 4), min(max_dy, coarse_dy + 4) + 1):
        for dx in range(max(0, coarse_dx - 4), min(max_dx, coarse_dx + 4) + 1):
            score = evaluate(dx, dy)
            if score < best[0]:
                best = (score, dx, dy)

    return best[1], best[2], best[0]


def rmse_rgb(lhs, rhs) -> float:
    import numpy as np

    diff = lhs - rhs
    return float(math.sqrt(float(np.mean(diff * diff))))


def clamp_region(region: tuple[int, int, int, int], width: int, height: int) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = region
    return max(0, x0), max(0, y0), min(width, x1), min(height, y1)


def build_regions(width: int, height: int) -> dict[str, tuple[int, int, int, int]]:
    return {
        "left": clamp_region((0, 0, 430, height), width, height),
        "right": clamp_region((width - 430, 0, width, height), width, height),
        "top": clamp_region((0, 0, width, 420), width, height),
        "bottom": clamp_region((0, height - 500, width, height), width, height),
        "file2_new": clamp_region((width - 480, 760, width - 160, 940), width, height),
    }


def write_side_by_side(path: Path, oracle_crop: Image.Image, candidate: Image.Image) -> None:
    canvas = Image.new("RGB", (oracle_crop.width + candidate.width, max(oracle_crop.height, candidate.height)))
    canvas.paste(oracle_crop, (0, 0))
    canvas.paste(candidate, (oracle_crop.width, 0))
    canvas.save(path)


def write_diff(path: Path, oracle_crop: Image.Image, candidate: Image.Image) -> None:
    import numpy as np

    lhs = np.asarray(oracle_crop, dtype=np.int16)
    rhs = np.asarray(candidate, dtype=np.int16)
    diff = np.abs(lhs - rhs).astype(np.uint8)
    Image.fromarray(diff, mode="RGB").save(path)


def main() -> int:
    args = parse_args()
    candidate_path = Path(args.candidate)
    oracle_path = Path(args.oracle)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    oracle = load_rgb(oracle_path)
    candidate = load_rgb(candidate_path)
    dx, dy, alignment_score = find_best_offset(oracle, candidate)
    oracle_crop = oracle.crop((dx, dy, dx + candidate.width, dy + candidate.height))

    oracle_rgb = rgb_array(oracle_crop)
    candidate_rgb = rgb_array(candidate)
    regions = build_regions(candidate.width, candidate.height)
    metrics = {
        "oracle": str(oracle_path),
        "candidate": str(candidate_path),
        "alignment": {"dx": dx, "dy": dy, "mae_luma": alignment_score},
        "metrics": {"full": rmse_rgb(oracle_rgb, candidate_rgb)},
        "regions": {},
    }

    for name, region in regions.items():
        x0, y0, x1, y1 = region
        region_oracle = oracle_rgb[y0:y1, x0:x1]
        region_candidate = candidate_rgb[y0:y1, x0:x1]
        metrics["regions"][name] = rmse_rgb(region_oracle, region_candidate)
        oracle_region = oracle_crop.crop(region)
        candidate_region = candidate.crop(region)
        write_diff(output_dir / f"{name}-diff.png", oracle_region, candidate_region)
        oracle_region.save(output_dir / f"{name}-oracle.png")
        candidate_region.save(output_dir / f"{name}-candidate.png")

    write_side_by_side(output_dir / "aligned-side-by-side.png", oracle_crop, candidate)
    write_diff(output_dir / "aligned-diff.png", oracle_crop, candidate)
    oracle_crop.save(output_dir / "aligned-oracle.png")
    candidate.save(output_dir / "aligned-candidate.png")

    with (output_dir / "metrics.json").open("w", encoding="utf-8") as handle:
        json.dump(metrics, handle, indent=2, sort_keys=True)
        handle.write("\n")

    summary_lines = [
        f"oracle: {oracle_path}",
        f"candidate: {candidate_path}",
        f"alignment: dx={dx} dy={dy} mae_luma={alignment_score:.4f}",
        f"full: {metrics['metrics']['full']:.4f}",
    ]
    for name in ("left", "right", "top", "bottom", "file2_new"):
        summary_lines.append(f"{name}: {metrics['regions'][name]:.4f}")

    summary = "\n".join(summary_lines) + "\n"
    (output_dir / "summary.txt").write_text(summary, encoding="utf-8")
    sys.stdout.write(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
