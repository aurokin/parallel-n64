# Fixture Manifests

Fixture manifests define deterministic emulator inputs and expected evidence.
Large assets remain untracked.

Each active fixture identifies:

- game and scene;
- ROM, state, pack, and configuration inputs;
- authority node and remint path;
- settle and capture points;
- required semantic and renderer evidence.

Start new definitions from
[`fixture-template.yaml`](fixture-template.yaml). Authority lineage
lives in
[`paper-mario-authority-graph.yaml`](paper-mario-authority-graph.yaml).
The runtime and evidence rules are in the
[scenario documentation](../scenarios/).

Add a fixture to the active catalog when its savestate or bootstrap is runnable.
Keep proposed gameplay routes in the gameplay lab until they provide a verified
state and a concrete renderer regression to exercise.

Tracked Paper Mario fixtures:

- [title screen](paper-mario-title-screen.yaml)
- [file select](paper-mario-file-select.yaml)
- [`kmr_03 ENTRY_2`](paper-mario-kmr-03-entry-2.yaml): an imported recording
  checkpoint with a pinned feature-off digest. It replaces the retired, scene-unqualified `kmr_03 ENTRY_5`
  fixture (see the issue log).
