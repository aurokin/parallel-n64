"""Contact sheets: one row of capture thumbnails per run, stacked for comparison."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

THUMB = (240, 180)
HEADER = 22
LABEL = 16
BACKGROUND = "#17191d"
TEXT = "#f0f0f0"
MISSING = "#3a2020"


def run_row(title: str, captures: list[tuple[int, Path | None]]) -> Image.Image:
    """`title` above one thumbnail per (frame, capture path); a missing path is a red cell."""
    width = THUMB[0] * max(len(captures), 1)
    row = Image.new("RGB", (width, HEADER + THUMB[1] + LABEL), BACKGROUND)
    draw = ImageDraw.Draw(row)
    draw.text((6, 5), title, fill=TEXT)
    for i, (frame, path) in enumerate(captures):
        x = i * THUMB[0]
        if path is not None and path.is_file():
            with Image.open(path) as image:
                thumb = image.convert("RGB")
            thumb.thumbnail(THUMB)
            row.paste(thumb, (x + (THUMB[0] - thumb.width) // 2, HEADER + (THUMB[1] - thumb.height) // 2))
        else:
            draw.rectangle((x + 1, HEADER + 1, x + THUMB[0] - 2, HEADER + THUMB[1] - 2), fill=MISSING)
        draw.text((x + 6, HEADER + THUMB[1] + 2), f"frame {frame}", fill=TEXT)
    return row


def stack(rows: list[Image.Image], gap: int = 4) -> Image.Image:
    width = max(r.width for r in rows)
    sheet = Image.new("RGB", (width, sum(r.height for r in rows) + gap * (len(rows) - 1)), "#000000")
    y = 0
    for row in rows:
        sheet.paste(row, (0, y))
        y += row.height + gap
    return sheet


def placeholder(title: str, note: str, width: int = THUMB[0] * 8) -> Image.Image:
    row = Image.new("RGB", (width, HEADER + THUMB[1] + LABEL), BACKGROUND)
    draw = ImageDraw.Draw(row)
    draw.text((6, 5), title, fill=TEXT)
    draw.text((6, HEADER + THUMB[1] // 2), note, fill=TEXT)
    return row


def labelled(label: str, image: Image.Image) -> Image.Image:
    """`image` under a strip naming its column, for side-by-side comparison sheets."""
    strip = Image.new("RGB", (image.width, HEADER), "#30343c")
    ImageDraw.Draw(strip).text((6, 5), label, fill=TEXT)
    return stack([strip, image], gap=0)


def save(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, quality=85)
