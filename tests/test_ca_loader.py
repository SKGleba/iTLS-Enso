import os
import pathlib
import re
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
LOADER = pathlib.Path(
    os.environ.get("ITLS_CA_LOADER", ROOT / "build" / "host-tests" / "ca_loader_test")
)
PEM_RE = re.compile(
    rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
    re.DOTALL,
)


class CaLoaderHarnessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        match = PEM_RE.search((ROOT / "misc" / "certs.cer").read_bytes())
        if match is None:
            raise AssertionError("repository CA bundle contains no certificate fixture")
        cls.certificate = match.group(0) + b"\n"

    def run_loader(self, data: bytes) -> subprocess.CompletedProcess[str]:
        with tempfile.NamedTemporaryFile() as bundle:
            bundle.write(data)
            bundle.flush()
            return subprocess.run(
                [str(LOADER), bundle.name],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_accepts_clean_end_of_file(self) -> None:
        result = self.run_loader(self.certificate)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("parsed=1 added=1", result.stdout)

    def test_rejects_truncated_certificate_at_end_of_file(self) -> None:
        result = self.run_loader(
            self.certificate + b"-----BEGIN CERTIFICATE-----\nAAAA\n"
        )

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("certificate parse failed after index 1", result.stderr)

    def test_rejects_duplicate_certificate(self) -> None:
        result = self.run_loader(self.certificate + self.certificate)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate certificate at index 2", result.stderr)


if __name__ == "__main__":
    unittest.main()
