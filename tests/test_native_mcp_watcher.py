from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import tools.native_mcp_watcher as watcher


class NativeMcpWatcherTests(unittest.TestCase):
    def run_git(self, repo: Path, *args: str) -> None:
        subprocess.run(["git", *args], cwd=repo, check=True, text=True, capture_output=True)

    def test_build_report_detects_changes_and_name_overlap(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ue_repo = root / "UnrealEngine"
            project = root / "ECABridge"
            native_dir = ue_repo / "Engine" / "Plugins" / "Experimental" / "ModelContextProtocol"
            eca_private = project / "Source" / "ECABridge" / "Private" / "Commands"
            eca_public = project / "Source" / "ECABridge" / "Public" / "Commands"
            native_dir.mkdir(parents=True)
            eca_private.mkdir(parents=True)
            eca_public.mkdir(parents=True)

            self.run_git(root, "init", "UnrealEngine")
            self.run_git(ue_repo, "config", "user.email", "agent@example.invalid")
            self.run_git(ue_repo, "config", "user.name", "Agent Test")
            (native_dir / "NativeTool.cpp").write_text('const char* Name = "old_tool";\n', encoding="utf-8")
            self.run_git(ue_repo, "add", ".")
            self.run_git(ue_repo, "commit", "-m", "initial native mcp")
            self.run_git(ue_repo, "tag", "base")
            (native_dir / "NativeTool.cpp").write_text('const char* Name = "find_assets";\n', encoding="utf-8")
            self.run_git(ue_repo, "commit", "-am", "add native find assets")
            self.run_git(ue_repo, "tag", "target")

            (eca_private / "ECAAssetCommands.cpp").write_text("REGISTER_ECA_COMMAND(FECACommand_FindAssets)\n", encoding="utf-8")
            (eca_public / "ECAAssetCommands.h").write_text(
                'class FECACommand_FindAssets { virtual FString GetName() const override { return TEXT("find_assets"); } };\n',
                encoding="utf-8",
            )

            args = SimpleNamespace(
                ue_repo=str(ue_repo),
                project_root=str(project),
                base="base",
                target="target",
                path=watcher.DEFAULT_NATIVE_PATHS,
                commit_limit=10,
                fetch=False,
            )
            report = watcher.build_report(args)

            self.assertTrue(report["ok"])
            self.assertEqual(report["changed_files"][0]["path"], "Engine/Plugins/Experimental/ModelContextProtocol/NativeTool.cpp")
            self.assertIn("find_assets", report["native_tool_candidates"])
            self.assertIn("find_assets", report["tool_name_overlap_with_ecabridge"])


if __name__ == "__main__":
    unittest.main()