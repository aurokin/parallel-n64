# Antigravity computer-use research brief (v3 cell design input)

Date: 2026-07-04 (overnight). Researched by a sonnet subagent (web-only, no
laptop access). Owner directive: v3 runs gemini-3.5-flash WITH Antigravity's
new computer-use feature. This brief is the pre-work; the go/no-go is
empirical on luma.

## Bottom line
Gemini 3.5 Flash has model-level computer use (browser + mobile + desktop
environments, "Supported (Preview)" on the model page), and Antigravity 2.0
runs on that model — but every piece of Antigravity-specific documentation
describes the product feature as **browser control via a Chrome extension +
native-messaging bridge to the IDE desktop-app process**, not raw OS-level
mouse/keyboard control, and not clearly available to the CLI (`agy`)
standalone. Whether `agy` can do computer use at all without the IDE app
running is UNDOCUMENTED — do a small empirical smoke on luma before
committing the round's single quota shot.

## Key findings
- **Surface split (load-bearing)**: the browser-control Chrome extension
  requires "Native Messaging: communicate with the Antigravity IDE process
  running locally"; all install flows are IDE-centric. `agy` CLI docs,
  changelog (v1.0.0–1.0.16), and cheat sheets contain no browser-control or
  computer-use flag/tool. (github.com/google-antigravity/antigravity-cli;
  antigravityide.help/blog/browser-automation-architecture;
  cloud.google.com "Choosing your surface" post)
- **Model level**: computer use natively integrated in gemini-3.5-flash
  (formerly gemini-2.5-computer-use-preview); environments:
  ENVIRONMENT_BROWSER / ENVIRONMENT_MOBILE / ENVIRONMENT_DESKTOP.
  (ai.google.dev/gemini-api/docs/computer-use) NOTE: Google's own
  "What's new in Gemini 3.5 Flash" FAQ contradicts this ("not supported") —
  unresolved doc conflict, sanity-check in the laptop's model picker.
- **macOS permissions**: no Antigravity doc names Screen Recording /
  Accessibility / Apple Events entitlements. The Chrome-extension
  architecture plausibly avoids the apple-events entitlement wall that broke
  codex CLI CU — but if ENVIRONMENT_DESKTOP is ever surfaced it would need
  Accessibility + Screen Recording TCC like everything else. Unconfirmed
  either way. Known forum workaround for unrelated shell-permission issues:
  Full Disk Access + Developer Tools for Antigravity.
- **Quota**: computer-use calls billed as regular tokens (no surcharge tier),
  but the perception-action loop (screenshot per turn) makes total burn much
  higher per task — no official multiplier documented. Antigravity quota =
  ~5h sprint limit + hard weekly cap (7-day lockout once exhausted; hit
  free/Pro/Ultra tiers since ~Mar 2026). Background file-indexing on save
  also draws quota — keep the eval workspace lean.
- **Headless/automation gotchas** (`agy` general, not CU-specific): non-TTY
  stdout drop bug with `-p` (final response silently dropped under pipes/CI);
  guidance: `--headless` + explicit `--approve <policy>` + stdin from
  /dev/null. Headless auth via GEMINI_API_KEY/ANTIGRAVITY_API_KEY env; open
  feature request #78 for first-class headless key auth. The Tauri desktop
  app fails on displayless hosts.

## Empirical smoke checklist (luma, BEFORE the scored v3 cell; owner present
for any TCC prompts; watch the quota meters during it)
1. Does standalone `agy` expose any computer-use/browser tool without the
   IDE app running?
2. If yes: does it work from tmux (console-born server), or only from a
   plain Terminal/Finder-launched context?
3. Any TCC prompts on first use, and do they surface in a terminal context?
4. Quota burn of a 2-minute CU task vs a text-only task (sprint counter
   delta) before committing the weekly cap.
5. Check the model picker / docs pages (antigravity.google/docs/cli-using,
   cli-permissions, browser — SPA pages unreadable to fetchers) for current
   authoritative wording.

## v3 cell-design implication
If Antigravity CU turns out to be browser-only, an "agy + computer use"
eval cell cannot drive RetroArch's window at all — the cell would need
either (a) the IDE desktop app as the surface (analogous to the codex
desktop-app manual-kickoff ruling), or (b) reclassification of the agy cell
back to axisB=unmentioned with CU noted as inapplicable. Decide after the
smoke.
