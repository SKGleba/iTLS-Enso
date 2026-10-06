import hashlib
import os
import pathlib
import re
import unittest

from tools.update_ca_bundle import (
    BundleError,
    build_bundle,
    generate_bundle_bytes,
    parse_certificates,
)


ROOT = pathlib.Path(__file__).resolve().parents[1]
SONY_SOURCE = pathlib.Path(
    os.environ.get("ITLS_SONY_SOURCE", ROOT / "misc" / "sony_certs.cer")
)
MOZILLA_SOURCE = pathlib.Path(
    os.environ.get("ITLS_MOZILLA_SOURCE", ROOT / "misc" / "mozilla_cacert.pem")
)
CA_BUNDLE = pathlib.Path(
    os.environ.get("ITLS_CA_BUNDLE", ROOT / "misc" / "certs.cer")
)
PEM_RE = re.compile(
    rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
    re.DOTALL,
)
ISRG_ROOT_X1_SHA256 = "96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6"
ISRG_ROOT_X2_SHA256 = "69729b8e15a86efc177a57afb7171dfc64add28c2fca8cf1507e34453ccb1470"


def repository_fixture_certificates(count: int) -> list[bytes]:
    blocks = PEM_RE.findall(CA_BUNDLE.read_bytes())
    if len(blocks) < count:
        raise AssertionError(f"need {count} certificate fixtures, found {len(blocks)}")
    return [block + b"\n" for block in blocks[:count]]


class UpdateCaBundleTests(unittest.TestCase):
    def test_parse_rejects_unbalanced_markers(self) -> None:
        with self.assertRaisesRegex(BundleError, "unbalanced"):
            parse_certificates("broken", b"-----BEGIN CERTIFICATE-----\nAAAA\n")

    def test_build_bundle_preserves_first_occurrence_and_source_order(self) -> None:
        first, second = repository_fixture_certificates(2)

        result = build_bundle(
            (
                ("sony", first + second),
                ("mozilla", second + first),
            )
        )

        expected = parse_certificates("expected", first + second)
        self.assertEqual(
            [certificate.der_sha256 for certificate in result.certificates],
            [certificate.der_sha256 for certificate in expected],
        )
        self.assertEqual(result.duplicate_count, 2)
        self.assertEqual([stat.input_count for stat in result.sources], [2, 2])
        self.assertEqual([stat.output_count for stat in result.sources], [2, 0])

    def test_generation_is_deterministic(self) -> None:
        first, second = repository_fixture_certificates(2)
        sources = (("sony", first), ("mozilla", second))

        self.assertEqual(generate_bundle_bytes(sources), generate_bundle_bytes(sources))

    def test_repository_bundle_is_reproducible(self) -> None:
        sources = (
            ("Sony/Vita", SONY_SOURCE.read_bytes()),
            ("Mozilla/curl", MOZILLA_SOURCE.read_bytes()),
        )

        generated = generate_bundle_bytes(sources)

        self.assertEqual(generated, CA_BUNDLE.read_bytes())

    def test_repository_bundle_has_unique_certificates_and_preserves_sony_prefix(self) -> None:
        sony = parse_certificates(
            "Sony/Vita", SONY_SOURCE.read_bytes()
        )
        combined = parse_certificates(
            "combined", CA_BUNDLE.read_bytes()
        )
        fingerprints = [certificate.der_sha256 for certificate in combined]

        self.assertEqual(len(sony), 67)
        self.assertEqual(
            fingerprints[: len(sony)],
            [certificate.der_sha256 for certificate in sony],
        )
        self.assertEqual(len(fingerprints), len(set(fingerprints)))
        self.assertIn(ISRG_ROOT_X1_SHA256, fingerprints)
        self.assertIn(ISRG_ROOT_X2_SHA256, fingerprints)

    def test_rendered_header_records_source_hashes(self) -> None:
        first, second = repository_fixture_certificates(2)
        sources = (("sony", first), ("mozilla", second))

        rendered = generate_bundle_bytes(sources)

        self.assertIn(hashlib.sha256(first).hexdigest().encode("ascii"), rendered)
        self.assertIn(hashlib.sha256(second).hexdigest().encode("ascii"), rendered)


if __name__ == "__main__":
    unittest.main()
