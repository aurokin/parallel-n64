"""Input patterns: a fixed list of timed presses expanded into a scenario program.

A pattern names N64 controls; `INPUTS` maps each to the RetroPad state that the
standard mupen64plus libretro mapping (ParaLLEish, upstream parallel-n64,
mupen64plus-next) turns into that control. Directions press the D-pad and fully
deflect the left stick together, because N64 menus read one or the other.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

SCHEMA = "n64-sweep-input/1"
FULL = 32767

# name -> (RetroPad joypad mask, (lx, ly, rx, ry)); libretro +Y is down.
INPUTS: dict[str, tuple[int, tuple[int, int, int, int]]] = {
    "a": (1 << 0, (0, 0, 0, 0)),        # RetroPad B
    "b": (1 << 1, (0, 0, 0, 0)),        # RetroPad Y
    "start": (1 << 3, (0, 0, 0, 0)),
    "up": (1 << 4, (0, -FULL, 0, 0)),
    "down": (1 << 5, (0, FULL, 0, 0)),
    "left": (1 << 6, (-FULL, 0, 0, 0)),
    "right": (1 << 7, (FULL, 0, 0, 0)),
    "l": (1 << 10, (0, 0, 0, 0)),
    "r": (1 << 11, (0, 0, 0, 0)),
    "z": (1 << 12, (0, 0, 0, 0)),       # RetroPad L2
    "c-up": (0, (0, 0, 0, -FULL)),      # right stick
    "c-down": (0, (0, 0, 0, FULL)),
    "c-left": (0, (0, 0, -FULL, 0)),
    "c-right": (0, (0, 0, FULL, 0)),
}


class PatternError(ValueError):
    """A pattern file does not satisfy the schema."""


def load(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as err:
        raise PatternError(f"{path}: unreadable pattern: {err}") from None
    validate(data, str(path))
    return data


def validate(data: Any, where: str = "pattern") -> None:
    def need(cond: bool, message: str) -> None:
        if not cond:
            raise PatternError(f"{where}: {message}")

    need(isinstance(data, dict) and data.get("schema") == SCHEMA, f"schema must be {SCHEMA!r}")
    frames = data.get("frames")
    need(isinstance(frames, int) and frames > 0, "frames must be a positive integer")
    captures = data.get("captures")
    need(isinstance(captures, list) and bool(captures), "captures must be a non-empty list")
    need(all(isinstance(f, int) and 0 < f <= frames for f in captures), f"captures must lie in 1..{frames}")
    need(captures == sorted(set(captures)), "captures must be strictly increasing")
    hold = data.get("hold_frames")
    need(isinstance(hold, int) and hold > 0, "hold_frames must be a positive integer")
    frozen = data.get("frozen_min_captures")
    need(isinstance(frozen, int) and 2 <= frozen <= len(captures),
         "frozen_min_captures must be between 2 and the number of captures")
    presses = data.get("presses")
    need(isinstance(presses, list), "presses must be a list")
    end = 0
    for press in presses:
        need(isinstance(press, dict) and isinstance(press.get("frame"), int), f"malformed press {press!r}")
        names = press_inputs(press)
        need(bool(names) and all(n in INPUTS for n in names), f"unknown input in {press!r}")
        combine(names)  # raises on conflicting stick axes
        duration = press.get("hold", hold)
        need(isinstance(duration, int) and duration > 0, f"hold must be positive in {press!r}")
        need(press["frame"] >= end, f"press at {press['frame']} overlaps the previous press")
        end = press["frame"] + duration
        need(end <= frames, f"press at {press['frame']} ends after frame {frames}")


def press_inputs(press: dict[str, Any]) -> list[str]:
    value = press.get("input")
    return [value] if isinstance(value, str) else list(value) if isinstance(value, list) else []


def combine(names: list[str]) -> tuple[int, tuple[int, ...]]:
    mask = 0
    axes = [0, 0, 0, 0]
    for name in names:
        bits, stick = INPUTS[name]
        mask |= bits
        for i, value in enumerate(stick):
            if value and axes[i] and axes[i] != value:
                raise PatternError(f"inputs {names} push one stick axis both ways")
            axes[i] = value or axes[i]
    return mask, tuple(axes)


def program(data: dict[str, Any]) -> list[str]:
    """The scenario program: idle steps, then set/step/clear around each press."""
    commands: list[str] = []
    frame = 0

    def advance(to: int) -> None:
        nonlocal frame
        if to > frame:
            commands.append(f"STEP_FRAME {to - frame}")
            frame = to

    for press in data["presses"]:
        advance(press["frame"])
        mask, axes = combine(press_inputs(press))
        stick = "" if not any(axes) else " " + " ".join(str(v) for v in axes)
        commands.append(f"SET_INPUT_PORT 0 0x{mask:x}{stick}")
        advance(press["frame"] + press.get("hold", data["hold_frames"]))
        commands.append("CLEAR_INPUT_PORT 0")
    advance(data["frames"])
    return commands


def digest(data: dict[str, Any]) -> str:
    """Identity of what the pattern does; descriptions do not change it."""
    behavior = {k: v for k, v in data.items() if k not in ("description", "notes")}
    behavior["presses"] = [{k: v for k, v in p.items() if k != "why"} for p in data["presses"]]
    return hashlib.sha256(json.dumps(behavior, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def for_rom(default: dict[str, Any], overrides: Path | None, rom_sha256: str) -> tuple[dict[str, Any], str]:
    """The pattern for one ROM: `<overrides>/<rom_sha256>.json` if present, else the default."""
    if overrides is not None:
        path = overrides / f"{rom_sha256}.json"
        if path.is_file():
            return load(path), f"override:{path.name}"
    return default, "default"
