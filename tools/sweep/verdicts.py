"""Mechanical verdicts for one boot-sweep run.

`classify` is a pure function of an observation (exit class, log flags, frames
reached, per-capture hash and blank flag) and the plan it ran (frames,
capture frames, frozen threshold). It says nothing about whether a title,
menu or gameplay was reached; contact sheets carry that judgment.
"""

from __future__ import annotations

from typing import Any

VERDICTS = ("pass", "frozen", "blank", "quit-fail", "hang", "crash", "load-fail", "error")

# run_scenarios exit classes that mean the harness, not the core, failed.
HARNESS_FAILURES = {"artifact-failed", "not-run", "capture-failed", "savefile-failed"}
# Exit classes of a run whose every step completed but whose frontend had to be forced down.
FORCED_QUIT = {"forced", "orphan-reaped"}
LOAD_FAILURE_TEXT = ("failed to load content", "failed to load rom", "rom rejected")


def load_failure_logged(log_text: str) -> bool:
    lowered = log_text.lower()
    return any(text in lowered for text in LOAD_FAILURE_TEXT)


def static_from(captures: list[dict[str, Any]]) -> tuple[int | None, int]:
    """Frame where the trailing run of identical captures starts, and its length.

    The frame is None unless at least two trailing captures are identical.
    """
    if not captures:
        return None, 0
    last = captures[-1]["sha256"]
    count = 0
    for capture in reversed(captures):
        if capture["sha256"] != last:
            break
        count += 1
    return (captures[-count]["frame"] if count >= 2 else None), count


def screen_class(captures: list[dict[str, Any]], frozen_min_captures: int) -> dict[str, Any]:
    """Visual class of a complete capture series: blank, frozen or pass."""
    first_frame, static_count = static_from(captures)
    nonblank = [c["frame"] for c in captures if not c["blank"]]
    result: dict[str, Any] = {"static_from_frame": first_frame, "static_captures": static_count,
                              "first_nonblank_frame": nonblank[0] if nonblank else None}
    if not nonblank:
        return {**result, "screen": "blank", "screen_sub": None}
    if static_count >= frozen_min_captures:
        sub = "static-from-start" if static_count == len(captures) else "advanced-then-static"
        return {**result, "screen": "frozen", "screen_sub": sub}
    return {**result, "screen": "pass", "screen_sub": None}


def classify(observation: dict[str, Any], plan: dict[str, Any]) -> dict[str, Any]:
    """Verdict, sub-class and supporting fields for one run.

    `observation`: exit_class, log_flags, frames_run, load_failure_logged and
    captures [{frame, sha256, blank}]. `plan`: frames, captures (frame list)
    and frozen_min_captures.
    """
    exit_class = observation["exit_class"]
    flags = set(observation.get("log_flags") or [])
    captures = observation.get("captures") or []
    complete = (observation["frames_run"] == plan["frames"]
                and [c["frame"] for c in captures] == list(plan["captures"]))
    screen = screen_class(captures, plan["frozen_min_captures"]) if complete else {
        "screen": None, "screen_sub": None, "static_from_frame": None,
        "static_captures": 0, "first_nonblank_frame": None}

    def verdict(name: str, sub: str | None, reason: str) -> dict[str, Any]:
        return {"verdict": name, "sub_class": sub, "reason": reason, "complete": complete, **screen}

    if exit_class == "load-failed" or observation.get("load_failure_logged"):
        return verdict("load-fail", None, "frontend could not load the content")
    if exit_class in HARNESS_FAILURES:
        return verdict("error", exit_class, observation.get("error") or exit_class)
    if complete:
        if exit_class == "clean":
            return verdict(screen["screen"], screen["screen_sub"], {
                "blank": "every capture is blank",
                "frozen": f"captures identical from frame {screen['static_from_frame']} to the end",
                "pass": "all frames and captures completed; no failure signal"}[screen["screen"]])
        # Every step finished; only shutdown failed. `screen` keeps the visual class.
        if exit_class in FORCED_QUIT or "stop-failed" in flags:
            sub = ("quit-timeout" if "quit-timeout" in flags else "stop-failed" if "stop-failed" in flags
                   else exit_class)
            return verdict("quit-fail", sub, "all frames completed; the frontend did not quit cleanly")
    if "ttl-expired" in flags:
        return verdict("hang", "ttl", f"session TTL expired at frame {observation['frames_run']}")
    if exit_class == "step-timeout":
        return verdict("hang", "step", f"stopped stepping at frame {observation['frames_run']}")
    if exit_class == "timeout":
        return verdict("hang", "unresponsive",
                       f"a command timed out at frame {observation['frames_run']} with the frontend alive")
    if exit_class == "nonzero":
        return verdict("crash", None, f"frontend exited at frame {observation['frames_run']}")
    return verdict("error", exit_class, f"unclassified exit {exit_class} at frame {observation['frames_run']}")


def capture_is_blank(image: Any) -> bool:
    """At most three colours, or at least 99.5% of pixels with max RGB <= 8.

    The tolerance ignores near-black dither and a few isolated bright pixels.
    `image` is a Pillow image.
    """
    rgb = image.convert("RGB")
    if rgb.getcolors(3) is not None:
        return True
    from PIL import ImageChops
    r, g, b = rgb.split()
    histogram = ImageChops.lighter(ImageChops.lighter(r, g), b).histogram()
    return sum(histogram[:9]) * 1000 >= rgb.width * rgb.height * 995
