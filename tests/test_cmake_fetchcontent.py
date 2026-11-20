from pathlib import Path
import unittest


CMAKE_PATH = Path(__file__).resolve().parents[1] / "CMakeLists.txt"


class SkellCoreFetchContentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cmake = CMAKE_PATH.read_text(encoding="utf-8")

    def test_pinned_skellcore_uses_supported_makeavailable_flow(self):
        self.assertIn("cmake_minimum_required(VERSION 3.22)", self.cmake)
        self.assertIn("GIT_TAG 4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d", self.cmake)
        self.assertIn("SOURCE_SUBDIR SkellCore", self.cmake)
        self.assertIn("FetchContent_MakeAvailable(SkellCoreRepo)", self.cmake)
        self.assertNotIn("FetchContent_Populate(SkellCoreRepo)", self.cmake)

    def test_local_skellcore_override_remains_available(self):
        self.assertIn('if(DEFINED SKELLCORE_PATH AND EXISTS "${SKELLCORE_PATH}/SkellCore/CMakeLists.txt")', self.cmake)
        self.assertIn('add_subdirectory("${SKELLCORE_PATH}/SkellCore"', self.cmake)


if __name__ == "__main__":
    unittest.main()
