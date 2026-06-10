#!/usr/bin/env python3
"""Stack two captures into a labeled review image.

For human/agent VISUAL review of GlideN64-reference vs paraLLEl hi-res
content only. Deliberately computes no similarity metric: numeric image
comparison against GlideN64 references is banned from the gate surface
(see emu_gliden64_reference_guardrails.sh).
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

LABEL_BAR_PX = 22


def load_labeled(path: Path, label: str, width: int) -> Image.Image:
    im = Image.open(path).convert("RGB")
    if im.width != width:
        im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
    bar = Image.new("RGB", (width, LABEL_BAR_PX), (24, 24, 24))
    draw = ImageDraw.Draw(bar)
    draw.text((6, 4), label, fill=(235, 235, 235))
    out = Image.new("RGB", (width, LABEL_BAR_PX + im.height))
    out.paste(bar, (0, 0))
    out.paste(im, (0, LABEL_BAR_PX))
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--top", required=True, help="Top capture path")
    parser.add_argument("--top-label", required=True)
    parser.add_argument("--bottom", required=True, help="Bottom capture path")
    parser.add_argument("--bottom-label", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    top_im = Image.open(args.top)
    width = top_im.width
    top = load_labeled(Path(args.top), args.top_label, width)
    bottom = load_labeled(Path(args.bottom), args.bottom_label, width)

    canvas = Image.new("RGB", (width, top.height + bottom.height))
    canvas.paste(top, (0, 0))
    canvas.paste(bottom, (0, top.height))
    canvas.save(args.output)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
