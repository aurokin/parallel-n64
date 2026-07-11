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

Use `status: planned` when a scene belongs in the authority graph but
does not yet have a runnable bootstrap or verified savestate.

Tracked Paper Mario fixtures:

- [title screen](paper-mario-title-screen.yaml)
- [file select](paper-mario-file-select.yaml)
- [`kmr_03 ENTRY_5`](paper-mario-kmr-03-entry-5.yaml)
- [`hos_05 ENTRY_3`](paper-mario-hos-05-entry-3.yaml), planned
