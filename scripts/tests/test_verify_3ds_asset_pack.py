from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
PACKER = ROOT / "scripts/pack_3ds_assets.py"
VERIFIER = ROOT / "scripts/verify_3ds_asset_pack.py"
spec = importlib.util.spec_from_file_location("verify_pack", VERIFIER)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class AssetPackIntegrityTests(unittest.TestCase):
    def make_pack(self, root: Path, resources: dict[str, Path]) -> Path:
        assets = root / "assets"
        assets.mkdir(exist_ok=True)
        output = root / "goldeneye.u.gepack"
        command = [
            sys.executable, str(PACKER), "--assets", str(assets),
            "--source-sha1", "abe01e4aeb033b6c0836819f549c791b26cfde83",
        ]
        for name, source in resources.items():
            command.extend(("--extra", f"{name}={source}"))
        command.extend(("--output", str(output)))
        subprocess.run(command, check=True, capture_output=True, text=True)
        return output

    def test_exact_required_contents_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            background = root / "background.bin"
            collision = root / "collision.gestan"
            weapon = root / "GwppksilZ.bin"
            background.write_bytes(b"background")
            collision.write_bytes(b"canonical collision")
            weapon.write_bytes(b"silenced pp7")
            resources = {
                "converted/levels/dam/background.bin": background,
                "converted/levels/dam/collision/collision.gestan": collision,
                "converted/models/first-person-pp7/GwppksilZ.bin": weapon,
            }
            pack = self.make_pack(root, resources)
            module.verify(pack, list(resources.items()))

    def test_missing_collision_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            background = root / "background.bin"
            collision = root / "collision.gestan"
            background.write_bytes(b"background")
            collision.write_bytes(b"canonical collision")
            pack = self.make_pack(root, {
                "converted/levels/dam/background.bin": background,
            })
            with self.assertRaisesRegex(
                    module.PackIntegrityError, "missing required asset"):
                module.verify(pack, [
                    ("converted/levels/dam/background.bin", background),
                    ("converted/levels/dam/collision/collision.gestan",
                     collision),
                ])

    def test_stale_required_content_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            collision = root / "collision.gestan"
            collision.write_bytes(b"first")
            name = "converted/levels/dam/collision/collision.gestan"
            pack = self.make_pack(root, {name: collision})
            collision.write_bytes(b"other")
            with self.assertRaisesRegex(
                    module.PackIntegrityError, "content mismatch"):
                module.verify(pack, [(name, collision)])


if __name__ == "__main__":
    unittest.main()
