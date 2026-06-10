from __future__ import annotations

import json
import unittest
from pathlib import Path

import tools.verify as verify


class VerifyEntrypointTests(unittest.TestCase):
    def test_project_files_includes_untracked_verify_script(self) -> None:
        paths = {path.relative_to(verify.REPO_ROOT).as_posix() for path in verify.project_files(["tools/verify.py"])}
        self.assertIn("tools/verify.py", paths)

    def test_uplugin_is_valid_json(self) -> None:
        plugin_path = verify.REPO_ROOT / "ECABridge.uplugin"
        payload = json.loads(plugin_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["Modules"][0]["Name"], "ECABridge")

    def test_artifact_root_is_under_repo(self) -> None:
        verify.ARTIFACT_ROOT.relative_to(verify.REPO_ROOT)


if __name__ == "__main__":
    unittest.main()