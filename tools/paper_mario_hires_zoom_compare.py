#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat


PROFILES = {
    "intro22": {
        "oracle": Path(
            "/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/"
            "hires/oracle-gliden64-4x-hires-on-intro22-matched-1/"
            "Paper Mario (USA)-260308-170727.png"
        ),
        "regions": {
            "top_banner": {"parallel_box": (500, 0, 2280, 360), "search_radius": 120, "oracle_bias": (8, 0)},
            "story_text": {"parallel_box": (760, 1480, 1900, 1880), "search_radius": 220, "oracle_bias": (8, 0)},
            "bottom_stage_grid": {"parallel_box": (420, 1460, 2360, 2086), "search_radius": 140, "oracle_bias": (8, 0)},
            "left_stage_grid": {"parallel_box": (0, 120, 420, 1560), "search_radius": 140, "oracle_bias": (8, 0)},
        },
    },
    "noinput16": {
        "oracle": Path(
            "/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/"
            "hires/oracle-gliden64-4x-hires-on-noinput-16s-1/Paper Mario (USA)-260308-011300.png"
        ),
        "regions": {
            "top_banner": {"parallel_box": (620, 10, 2140, 340), "search_radius": 120},
            "today_text": {"parallel_box": (1040, 1590, 1700, 1825), "search_radius": 180},
            "bottom_stage_grid": {"parallel_box": (540, 1480, 2240, 2022), "search_radius": 140},
            "left_stage_grid": {"parallel_box": (0, 140, 360, 1510), "search_radius": 140},
        },
    },
}
DEFAULT_PROFILE = "intro22"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create focused zoom comparisons between a Paper Mario parallel HIRES capture and the saved GLide oracle."
    )
    parser.add_argument("--candidate", required=True, help="Candidate PNG to compare.")
    parser.add_argument(
        "--profile",
        default=DEFAULT_PROFILE,
        choices=sorted(PROFILES.keys()),
        help="Comparison scene profile to use.",
    )
    parser.add_argument("--oracle", default="", help="Oracle PNG to compare against.")
    parser.add_argument("--output-dir", required=True, help="Directory to write crops and summaries into.")
    return parser.parse_args()


def load_rgb(path: Path) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(f"missing image: {path}")
    return Image.open(path).convert("RGB")


def grayscale(image: Image.Image) -> Image.Image:
    return image.convert("L")


def mean_abs_diff(lhs: Image.Image, rhs: Image.Image) -> float:
    diff = ImageChops.difference(lhs, rhs)
    return float(sum(ImageStat.Stat(diff).mean))


def find_best_box(candidate_gray: Image.Image, oracle_gray: Image.Image, pbox: tuple[int, int, int, int], radius: int) -> tuple[int, int, int, int, float]:
    px0, py0, px1, py1 = pbox
    target = candidate_gray.crop(pbox)
    width = px1 - px0
    height = py1 - py0
    step = 4
    best_score = None
    best_box = None

    for dx in range(-radius, radius + 1, step):
        for dy in range(-radius, radius + 1, step):
            gx0 = px0 + dx
            gy0 = py0 + dy
            gx1 = gx0 + width
            gy1 = gy0 + height
            if gx0 < 0 or gy0 < 0 or gx1 > oracle_gray.width or gy1 > oracle_gray.height:
                continue
            oracle_crop = oracle_gray.crop((gx0, gy0, gx1, gy1))
            score = mean_abs_diff(target, oracle_crop)
            if best_score is None or score < best_score:
                best_score = score
                best_box = (gx0, gy0, gx1, gy1)

    if best_box is None or best_score is None:
        raise RuntimeError(f"failed to align region {pbox}")
    return (*best_box, best_score)


def apply_oracle_bias(
    oracle_box: tuple[int, int, int, int],
    oracle_image: Image.Image,
    bias: tuple[int, int],
) -> tuple[int, int, int, int]:
    dx, dy = bias
    gx0, gy0, gx1, gy1 = oracle_box
    width = gx1 - gx0
    height = gy1 - gy0
    gx0 += dx
    gy0 += dy
    gx0 = min(max(0, gx0), oracle_image.width - width)
    gy0 = min(max(0, gy0), oracle_image.height - height)
    return (gx0, gy0, gx0 + width, gy0 + height)


def build_row(title: str, parallel_crop: Image.Image, glide_crop: Image.Image) -> Image.Image:
    scale = 2 if max(parallel_crop.width, parallel_crop.height) < 900 else 1
    if scale != 1:
      parallel_crop = parallel_crop.resize((parallel_crop.width * scale, parallel_crop.height * scale), Image.Resampling.NEAREST)
      glide_crop = glide_crop.resize((glide_crop.width * scale, glide_crop.height * scale), Image.Resampling.NEAREST)

    gap = 24
    header_h = 28
    row = Image.new(
        "RGB",
        (parallel_crop.width + glide_crop.width + gap, max(parallel_crop.height, glide_crop.height) + header_h + 8),
        "black",
    )
    row.paste(parallel_crop, (0, header_h + 8))
    row.paste(glide_crop, (parallel_crop.width + gap, header_h + 8))
    draw = ImageDraw.Draw(row)
    draw.text((0, 0), f"{title}: parallel", fill="white")
    draw.text((parallel_crop.width + gap, 0), "glide", fill="white")
    return row


def build_diff(lhs: Image.Image, rhs: Image.Image) -> Image.Image:
    diff = ImageChops.difference(lhs, rhs)
    bands = [band.point(lambda x: min(255, x * 8)) for band in diff.split()]
    return Image.merge("RGB", bands)


def fit_label(text: str, limit: int = 120) -> str:
    if len(text) <= limit:
        return text
    keep = max(16, (limit - 3) // 2)
    return text[:keep] + "..." + text[-keep:]


def main() -> int:
    args = parse_args()
    candidate_path = Path(args.candidate)
    profile = PROFILES[args.profile]
    oracle_path = Path(args.oracle) if args.oracle else profile["oracle"]
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    candidate = load_rgb(candidate_path)
    oracle = load_rgb(oracle_path)
    candidate_gray = grayscale(candidate)
    oracle_gray = grayscale(oracle)

    summary = {
        "candidate": str(candidate_path),
        "oracle": str(oracle_path),
        "profile": args.profile,
        "regions": {},
    }

    rows = []
    for name, spec in profile["regions"].items():
        pbox = spec["parallel_box"]
        gx0, gy0, gx1, gy1, score = find_best_box(candidate_gray, oracle_gray, pbox, spec["search_radius"])
        gbox = (gx0, gy0, gx1, gy1)
        if "oracle_bias" in spec:
            gbox = apply_oracle_bias(gbox, oracle, spec["oracle_bias"])

        parallel_crop = candidate.crop(pbox)
        glide_crop = oracle.crop(gbox)
        row = build_row(name, parallel_crop, glide_crop)
        row.save(output_dir / f"{name}.png")
        build_diff(parallel_crop, glide_crop).save(output_dir / f"{name}-diff.png")
        parallel_crop.save(output_dir / f"{name}-parallel.png")
        glide_crop.save(output_dir / f"{name}-glide.png")
        rows.append(row)

        summary["regions"][name] = {
            "parallel_box": list(pbox),
            "glide_box": list(gbox),
            "alignment_score": score,
        }

    width = max(row.width for row in rows)
    gap = 24
    header_lines = [
        f"profile: {args.profile}",
        f"candidate: {fit_label(str(candidate_path))}",
        f"oracle: {fit_label(str(oracle_path))}",
    ]
    header_h = 18 * len(header_lines) + 24
    height = header_h + sum(row.height for row in rows) + gap * (len(rows) - 1)
    summary_image = Image.new("RGB", (width, height), "black")
    draw = ImageDraw.Draw(summary_image)
    y = 8
    for line in header_lines:
        draw.text((0, y), line, fill="white")
        y += 18
    y = header_h
    for row in rows:
        summary_image.paste(row, (0, y))
        y += row.height + gap
    summary_image.save(output_dir / "summary.png")

    summary_text_lines = [
        f"candidate: {candidate_path}",
        f"oracle: {oracle_path}",
        f"profile: {args.profile}",
    ]
    for name in profile["regions"]:
        region = summary["regions"][name]
        summary_text_lines.append(
            f"{name}: parallel={tuple(region['parallel_box'])} glide={tuple(region['glide_box'])} score={region['alignment_score']:.4f}"
        )
    summary_text = "\n".join(summary_text_lines) + "\n"
    (output_dir / "summary.txt").write_text(summary_text, encoding="utf-8")
    (output_dir / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(summary_text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
