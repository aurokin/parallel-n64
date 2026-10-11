"""Check content identity and overlay provenance through the actual promotion CLI."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


PROMOTER = Path(__file__).resolve().parents[3] / "tools/adapters/promote_interactive_state.py"


def main():
    with tempfile.TemporaryDirectory(prefix="state-promotion-") as directory:
        root = Path(directory)
        bundle = root / "session"
        states = bundle / "states/ParaLLEl N64"
        states.mkdir(parents=True)
        state = states / "Super Mario 64 (USA).state4"
        state.write_bytes(b"offline state bytes; no runtime qualification")
        captures = bundle / "captures"
        captures.mkdir()
        (captures / "capture.png").write_bytes(b"offline capture bytes")
        overlay = bundle / "retroarch.extra.append.cfg"
        overlay.write_bytes(b'video_vsync = "false"\n')
        digest = hashlib.sha256(overlay.read_bytes()).hexdigest()
        metadata = bundle / "retroarch.session.env"
        output = root / "promoted"
        target = output / "sm64-candidate"
        command = [sys.executable, str(PROMOTER), "--bundle-dir", str(bundle),
                   "--slot", "4", "--id", target.name, "--output-root", str(output)]

        metadata.write_text("")
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode != 0 and "ROM_PATH identity" in result.stderr
        assert not output.exists(), "missing identity must fail before output mutation"

        env = ("ROM_PATH=/declared/content/Super Mario 64 (USA).z64\n"
               "EXTRA_APPEND_CONFIG=/original/source/overlay.cfg\n"
               "EXTRA_APPEND_CONFIG_SNAPSHOT=/old/session/retroarch.extra.append.cfg\n"
               f"EXTRA_APPEND_CONFIG_SHA256={digest}\n")
        metadata.write_text(env)
        subprocess.run(command, check=True, capture_output=True, text=True)
        manifest = json.loads((target / "manifest.json").read_text())
        promoted_state = Path(manifest["promoted"]["state_path"])
        assert promoted_state.name == "Super Mario 64 (USA).state"
        assert promoted_state.read_bytes() == state.read_bytes()
        copied = next(item for item in manifest["copied_configs"]
                      if item["session_key"] == "EXTRA_APPEND_CONFIG_SNAPSHOT")
        assert Path(copied["bundle_path"]).read_bytes() == overlay.read_bytes()
        assert copied["sha256"] == manifest["hashes"]["extra_append_config_sha256"] == digest
        assert manifest["runtime_identity"]["extra_append_config_source"] == "/original/source/overlay.cfg"

        before = (target / "manifest.json").read_bytes()
        sentinel = root / "sentinel"
        sentinel.write_bytes(b"preserve output parent")
        for invalid_id in (".", ".."):
            invalid_command = command.copy()
            invalid_command[command.index("--id") + 1] = invalid_id
            result = subprocess.run([*invalid_command, "--force"], capture_output=True, text=True)
            assert result.returncode != 0 and "dot components" in result.stderr
            assert sentinel.read_bytes() == b"preserve output parent"
            assert (target / "manifest.json").read_bytes() == before
        outside = root / "outside"
        outside.mkdir()
        outside_sentinel = outside / "sentinel"
        outside_sentinel.write_bytes(b"preserve symlink target")
        (output / "escape").symlink_to(outside, target_is_directory=True)
        invalid_command = command.copy()
        invalid_command[command.index("--id") + 1] = "escape"
        result = subprocess.run([*invalid_command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "symlink" in result.stderr
        assert outside_sentinel.read_bytes() == b"preserve symlink target"
        overlap_command = command.copy()
        overlap_command[command.index("--id") + 1] = bundle.name
        overlap_command[command.index("--output-root") + 1] = str(root)
        result = subprocess.run([*overlap_command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "contains source inputs" in result.stderr
        assert state.read_bytes() == b"offline state bytes; no runtime qualification"
        assert overlay.is_file()
        state_bytes = state.read_bytes()
        state.unlink()
        state.mkdir()
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "not a regular file" in result.stderr
        assert (target / "manifest.json").read_bytes() == before
        state.rmdir()
        external_state = target / "external.state4"
        external_state.write_bytes(state_bytes)
        state.symlink_to(external_state)
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "contains source inputs" in result.stderr
        assert external_state.read_bytes() == state_bytes
        state.unlink()
        state.write_bytes(state_bytes)
        external_overlay = target / "external.cfg"
        external_overlay.write_bytes(overlay.read_bytes())
        overlay.unlink()
        overlay.symlink_to(external_overlay)
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "contains source inputs" in result.stderr
        assert external_overlay.is_file()
        overlay.unlink()
        overlay.write_bytes(external_overlay.read_bytes())
        for key in ("BASE_CONFIG", "APPEND_CONFIG", "CORE_OPTIONS_FILE", "CORE_OPTIONS_LAUNCH_FILE", "SAVEFILE_SOURCE"):
            metadata.write_text(env + f"{key}={external_overlay}\n")
            result = subprocess.run([*command, "--force"], capture_output=True, text=True)
            assert result.returncode != 0 and "contains source inputs" in result.stderr
            assert external_overlay.is_file()
            assert (target / "manifest.json").read_bytes() == before
        metadata.write_text(env.replace("EXTRA_APPEND_CONFIG_SNAPSHOT=/old/session/retroarch.extra.append.cfg\n", ""))
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "snapshot is missing from recorded metadata" in result.stderr
        assert (target / "manifest.json").read_bytes() == before
        metadata.write_text(env.replace(f"EXTRA_APPEND_CONFIG_SHA256={digest}\n", ""))
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "recorded SHA-256 identity" in result.stderr
        assert (target / "manifest.json").read_bytes() == before
        metadata.write_text(env.replace(digest, "invalid-hash"))
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "recorded SHA-256 identity" in result.stderr
        assert (target / "manifest.json").read_bytes() == before
        metadata.write_text(env)
        overlay.write_bytes(b"changed snapshot")
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "hash differs" in result.stderr
        assert (target / "manifest.json").read_bytes() == before
        overlay.unlink()
        result = subprocess.run([*command, "--force"], capture_output=True, text=True)
        assert result.returncode != 0 and "snapshot is missing" in result.stderr
        assert (target / "manifest.json").read_bytes() == before
    print("PASS: multi-game identity, moved overlay snapshot, and fail-before-mutation provenance")


if __name__ == "__main__":
    main()
