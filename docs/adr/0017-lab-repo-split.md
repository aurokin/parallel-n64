# ADR-0017: Experimental Session Tooling Stays External

- Status: Accepted, amended
- Date: 2026-06-12; boundary generalized 2026-07-10

## Context

Long gameplay sessions and one-off forensics generate useful scripts, but they
carry volatile routes, local assets, and machine assumptions. Keeping them in
the product repository confuses experiments with renderer authority.

## Decision

- Product adapters, fixtures, scenarios, converters, patch backups, and tests
  stay in `parallel-n64`.
- Long playthroughs, route notes, macro experiments, durable gameplay indexes,
  and disposable analysis stay in a separate private research system.
- Nothing in this repository may depend on that system.
- Private topology and fleet operation likewise remain outside the public
  product tree.

Promote an experimental tool here only when it becomes a portable product
entrypoint with a focused gate.

The existing `tools/adapters/vision_tools_mcp.py` is a migration exception,
not precedent: it remains untouched until its eval consumer seam moves with it.

## Consequences

Git history and external evidence preserve experimental methodology without
making it part of onboarding or correctness policy.

## Evidence

The repository contains only the portable adapter/fixture/scenario layer; its
required gate has no private-research dependency.
