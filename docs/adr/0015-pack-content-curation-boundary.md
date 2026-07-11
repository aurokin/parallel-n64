# ADR-0015: Miss Taxonomy And Pack-Curation Boundary

- Status: Accepted, amended
- Date: 2026-06-11; amended 2026-06-13

## Context

Replacement divergence may come from identity, missing pack content,
non-authoritative upload lookup, renderer composition, or the art itself.
Treating every miss as a renderer defect creates heuristic serving.

## Decision

Classify misses as:

- **family-variant miss** — the pack has the family but not the exact variant;
- **true pack gap** — no matching authored content, so explicit native fallback
  is correct;
- **upload-lane noise** — upload lookup misses but authoritative draw-time
  lookup resolves.

When the same pack entry is demonstrably wrong on its authoring renderer, the
remedy is pack curation or explicit fallback, never a scene-specific renderer
override.

The original “sewer slate collision” example was overturned. Those artifacts
were renderer composition and view-keying defects fixed by
[ADR-0018](0018-hires-composition-pack-contract.md), not content to curate.
The I-format/tlut semantics question is also closed: serve authored replacement
RGBA verbatim and apply the class-level composition rules.

## Consequences

Draw-time attempted keys and explicit fallback reasons are the useful census.
Checksums alone cannot decide whether the renderer, identity, or content is at
fault.

## Evidence

- attempted-key logging;
- tracked Paper Mario exclusion reviews;
- the ADR-0018 composition evidence chain.
