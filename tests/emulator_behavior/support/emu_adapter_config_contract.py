"""Check standalone adapter argv, config snapshots, and environment plumbing."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import unittest

REPO = Path(__file__).resolve().parents[3]
MVK_ENV = "MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS"
SHADER_ENV = "PARALLEL_RDP_DISABLE_HIRES_SHADER"

# Adopt detached interactive leaders so the test can reap them instead of
# depending on the container's init to reap orphaned children.
if sys.platform.startswith("linux"):
    if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), "PR_SET_CHILD_SUBREAPER failed")

FAKE_FRONTEND = r'''#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys

if "--command" in sys.argv:
    print("[ERROR] [NetCMD] \tValid commands:")
    for command in ("LOAD_STATE_SLOT <slot number>",
                    "LOAD_STATE_SLOT_PAUSED <slot number>", "WAIT_LOAD_STATE No argument"):
        print("[ERROR] [NetCMD] \t\t" + command)
    raise SystemExit(1)
argv = sys.argv[1:]
append = argv[argv.index("--appendconfig") + 1]
Path(os.environ["FAKE_FRONTEND_REPORT"]).write_text(json.dumps({
    "argv": argv,
    "env": {key: os.environ[key] for key in (
        "MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "PARALLEL_RDP_DISABLE_HIRES_SHADER")
        if key in os.environ},
    "append_files": [(path, Path(path).read_text()) for path in append.split("|")],
}))
for command in sys.stdin:
    if command.strip() == "PING":
        print("PING OK", flush=True)
    elif command.strip() == "QUIT":
        break
'''


class AdapterConfigContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="adapter config ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        # Copies prove exported adapters do not need the original checkout.
        self.adapters = self.root / "export/tools/adapters"
        self.adapters.mkdir(parents=True)
        for name in ("retroarch_interactive_session.sh", "retroarch_stdin_session.sh"):
            shutil.copyfile(REPO / "tools/adapters" / name, self.adapters / name)
        self.bin_dir = self.root / "bin"
        self.bin_dir.mkdir()
        self.uname = self.bin_dir / "uname"
        self.uname.write_text('#!/bin/sh\nprintf "%s\\n" "$FAKE_PLATFORM"\n')
        self.uname.chmod(0o755)
        self.base = self.root / "explicit base.cfg"
        self.base.write_text('video_driver = "base-value"\n')
        self.extra = self.root / "operator extra:config.cfg"
        self.extra.write_text('video_driver = "caller-value"\nconfig_contract_marker = "extra"\n')
        for name in ("rom", "core", "pack.phrb"):
            (self.root / name).write_text("offline fixture")
        self.env = {
            "PATH": str(self.bin_dir) + os.pathsep + os.environ["PATH"],
            "TMPDIR": str(self.root), "EXIT_WAIT": "1",
            "FAKE_PLATFORM": "Linux", "RETROARCH_TTL_GRACE_SECONDS": "0",
            "PARALLEL_RDP_HIRES_CACHE_PATH": str(self.root / "pack.phrb"),
        }
        self.counter = 0

    def frontend(self, app_name):
        binary = self.root / app_name
        if app_name.endswith(".app"):
            binary = binary / "Contents/MacOS/RetroArch"
            nib = binary.parent.parent / "Resources/en.lproj/MainMenu.nib"
            nib.parent.mkdir(parents=True, exist_ok=True)
            nib.touch()
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_text(FAKE_FRONTEND)
        binary.chmod(0o755)
        return binary

    def invoke(self, adapter, args, env):
        return subprocess.run(["bash", str(self.adapters / adapter), *args],
                              cwd="/", env=env, text=True, capture_output=True, timeout=20)

    def stop_interactive(self, bundle, env):
        pid_file = bundle / "session.pid"
        if not pid_file.exists():
            return
        pgid = int(pid_file.read_text())
        reaper = None
        if sys.platform.startswith("linux"):
            def reap():
                try:
                    os.waitpid(pgid, 0)
                except ChildProcessError:
                    pass
            reaper = threading.Thread(target=reap, daemon=True)
            reaper.start()
        try:
            result = self.invoke("retroarch_interactive_session.sh",
                                 ["stop", "--bundle-dir", str(bundle)], env)
            if result.returncode:
                raise AssertionError(result.stderr)
        finally:
            try:
                os.killpg(pgid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            if reaper:
                reaper.join(timeout=5)
                if reaper.is_alive():
                    raise AssertionError("interactive session leader did not exit")

    def launch(self, kind, platform, app_name, mode="off", mvk=None,
               explicit_base=True, extra=True, shader=None, explicit_frontend=True):
        self.counter += 1
        bundle = self.root / f"bundle {self.counter}"
        report_path = self.root / f"report {self.counter}.json"
        env = {**self.env, "FAKE_PLATFORM": platform,
               "FAKE_FRONTEND_REPORT": str(report_path)}
        if mvk is not None:
            env[MVK_ENV] = mvk
        if shader is not None:
            env[SHADER_ENV] = shader
        binary = self.frontend(app_name)
        args = ["--bundle-dir", str(bundle), "--rom", str(self.root / "rom"),
                "--core", str(self.root / "core"), "--mode", mode]
        if explicit_frontend:
            args += ["--retroarch-bin", str(binary)]
        if explicit_base:
            args += ["--base-config", str(self.base)]
            # An explicit argument must win over this environment value.
            env["RETROARCH_BASE_CONFIG"] = str(self.root / "unused.cfg")
        else:
            env["RETROARCH_BASE_CONFIG"] = str(self.base)
        if extra:
            args += ["--extra-append-config", str(self.extra)]
        if kind == "interactive":
            adapter = "retroarch_interactive_session.sh"
            args = ["start", *args, "--ttl-seconds", "15"]
            self.addCleanup(self.stop_interactive, bundle, env)
        else:
            adapter = "retroarch_stdin_session.sh"
            args += ["--startup-wait", "0", "--command", "QUIT"]
        result = self.invoke(adapter, args, env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(report_path.read_text())
        fields = dict(line.split("=", 1) for line in
                      (bundle / "retroarch.session.env").read_text().splitlines())
        self.assertEqual(fields["RETROARCH_BIN"], str(binary))
        argv = report["argv"]
        self.assertEqual(argv.count("--config"), 1)
        self.assertEqual(argv[argv.index("--config") + 1], str(self.base))
        self.assertEqual(argv.count("--appendconfig"), 1)
        generated = bundle / "retroarch.append.cfg"
        expected_paths = [str(generated)]
        if extra:
            snapshot = bundle / "retroarch.extra.append.cfg"
            expected_paths.append(str(snapshot))
            self.assertEqual(self.extra.read_text(),
                             'video_driver = "caller-value"\nconfig_contract_marker = "extra"\n')
            self.assertEqual(snapshot.read_bytes(), self.extra.read_bytes())
            self.assertEqual(fields["EXTRA_APPEND_CONFIG"], str(self.extra))
            self.assertEqual(fields["EXTRA_APPEND_CONFIG_SNAPSHOT"], str(snapshot))
            self.assertEqual(fields["EXTRA_APPEND_CONFIG_SHA256"],
                             hashlib.sha256(snapshot.read_bytes()).hexdigest())
            self.assertNotIn('config_contract_marker', generated.read_text())
            self.extra.write_text("changed after launch\n")
            self.assertEqual(snapshot.read_text(), report["append_files"][1][1])
            self.extra.write_text('video_driver = "caller-value"\nconfig_contract_marker = "extra"\n')
        else:
            self.assertEqual(fields["EXTRA_APPEND_CONFIG_SNAPSHOT"], "")
            self.assertEqual(fields["EXTRA_APPEND_CONFIG_SHA256"], "")
        self.assertEqual(argv[argv.index("--appendconfig") + 1], "|".join(expected_paths))
        self.assertEqual(fields["APPEND_CONFIG_ARGUMENT"], "|".join(expected_paths))
        self.assertEqual([path for path, _ in report["append_files"]], expected_paths)
        self.assertEqual(fields["APPEND_CONFIG_SHA256"],
                         hashlib.sha256(generated.read_bytes()).hexdigest())
        self.assertIn('video_driver = "vulkan"', generated.read_text())
        self.assertIn(f'video_fullscreen = "{"false" if platform == "Darwin" else "true"}"',
                      generated.read_text())
        self.assertEqual(self.base.read_text(), 'video_driver = "base-value"\n')
        expected_env = {} if mvk is None else {MVK_ENV: mvk}
        if shader is not None:
            expected_env[SHADER_ENV] = shader
        elif platform == "Darwin" and mode == "off":
            expected_env[SHADER_ENV] = "1"
        self.assertEqual(report["env"], expected_env)
        if kind == "interactive":
            # Release the lock before the next launch in a matrix test.
            self.stop_interactive(bundle, env)
            (bundle / "session.pid").unlink()

    def test_explicit_config_composition_on_both_platforms_and_adapters(self):
        for kind in ("batch", "interactive"):
            for platform in ("Linux", "Darwin"):
                with self.subTest(kind=kind, platform=platform):
                    self.launch(kind, platform, "ordinary frontend.app", mvk="caller sentinel")

    def test_environment_base_config_and_single_append_file(self):
        for kind in ("batch", "interactive"):
            for platform in ("Linux", "Darwin"):
                with self.subTest(kind=kind, platform=platform):
                    self.launch(kind, platform, "ordinary frontend.app",
                                explicit_base=False, extra=False)

    def test_linux_default_frontend_discovery(self):
        for kind in ("batch", "interactive"):
            with self.subTest(kind=kind):
                self.launch(kind, "Linux", "bin/retroarch", explicit_frontend=False,
                            explicit_base=False, extra=False)

    def test_macos_app_name_and_mode_preserve_absent_or_empty_mvk(self):
        for kind in ("batch", "interactive"):
            for mode in ("off", "on"):
                for app_name, mvk in (("ordinary frontend.app", None),
                                      ("RetroArch-MVK141.app", None),
                                      ("RetroArch-MVK141.app", "")):
                    with self.subTest(kind=kind, mode=mode, app_name=app_name, mvk=mvk):
                        self.launch(kind, "Darwin", app_name, mode=mode, mvk=mvk)

    def test_batch_bare_darwin_binary_preserves_absent_mvk(self):
        self.launch("batch", "Darwin", "bare-frontend", mode="on")

    def test_caller_shader_override_survives_macos_feature_off(self):
        for kind in ("batch", "interactive"):
            with self.subTest(kind=kind):
                self.launch(kind, "Darwin", "ordinary frontend.app", shader="0")

    def test_ambiguous_append_paths_fail_before_staging_or_frontend_probe(self):
        binary = self.frontend("ordinary frontend.app")
        for kind in ("batch", "interactive"):
            for bad in ("bundle", "extra"):
                with self.subTest(kind=kind, bad=bad):
                    bundle = self.root / ("bundle|ambiguous" if bad == "bundle" else "must-not-exist")
                    extra = self.root / "extra|ambiguous.cfg" if bad == "extra" else self.extra
                    extra.write_text('video_driver = "caller-value"\n')
                    args = ["--bundle-dir", str(bundle), "--rom", str(self.root / "rom"),
                            "--core", str(self.root / "core"), "--base-config", str(self.base),
                            "--retroarch-bin", str(binary), "--extra-append-config", str(extra)]
                    adapter = f"retroarch_{'interactive' if kind == 'interactive' else 'stdin'}_session.sh"
                    if kind == "interactive":
                        args.insert(0, "start")
                    result = self.invoke(adapter, args, self.env)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("must not contain '|'", result.stderr)
                    self.assertFalse(bundle.exists())

    def test_missing_base_config_fails_before_staging(self):
        binary = self.frontend("ordinary frontend.app")
        for kind in ("batch", "interactive"):
            for platform in ("Linux", "Darwin"):
                with self.subTest(kind=kind, platform=platform):
                    bundle = self.root / "no default config bundle"
                    args = ["--bundle-dir", str(bundle), "--rom", str(self.root / "rom"),
                            "--core", str(self.root / "core"), "--retroarch-bin", str(binary)]
                    adapter = f"retroarch_{'interactive' if kind == 'interactive' else 'stdin'}_session.sh"
                    if kind == "interactive":
                        args.insert(0, "start")
                    result = self.invoke(adapter, args, {**self.env, "FAKE_PLATFORM": platform})
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertFalse(bundle.exists())

    def test_interactive_bare_darwin_binary_fails_before_staging(self):
        bundle = self.root / "bare bundle"
        result = self.invoke("retroarch_interactive_session.sh", [
            "start", "--bundle-dir", str(bundle), "--rom", str(self.root / "rom"),
            "--core", str(self.root / "core"), "--base-config", str(self.base),
            "--retroarch-bin", str(self.frontend("bare-frontend"))],
            {**self.env, "FAKE_PLATFORM": "Darwin"})
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("complete RetroArch .app bundle", result.stderr)
        self.assertFalse(bundle.exists())


if __name__ == "__main__":
    unittest.main()
