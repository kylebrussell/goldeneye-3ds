import pathlib
import re
import unittest


class FadeRendererStateTest(unittest.TestCase):
    def test_final_colour_screen_restores_untextured_primitive_combine(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        source = (root / "platform/3ds/source/main.c").read_text()
        match = re.search(
            r"if \(fade_overlay\.visible\) \{(?P<body>.*?)\n    \}",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(match)
        body = match.group("body")
        draw = body.index("C3D_DrawArrays")
        self.assertLess(body.index("C3D_TexEnvInit"), draw)
        self.assertLess(body.index("GPU_PRIMARY_COLOR"), draw)
        self.assertLess(body.index("GPU_REPLACE"), draw)


if __name__ == "__main__":
    unittest.main()
