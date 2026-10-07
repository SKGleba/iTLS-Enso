import hashlib
import os
import pathlib
import re
import stat
import tempfile
import unittest

from tools.update_ca_bundle import (
    BundleError,
    _write_atomic,
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
STOCK_373_CA_LIST_SHA256 = (
    "14cafb9d12c1a604564e85326e057e459055c62d2b6f90add38dcb6e4592a396"
)


def repository_fixture_certificates(count: int) -> list[bytes]:
    blocks = PEM_RE.findall(CA_BUNDLE.read_bytes())
    if len(blocks) < count:
        raise AssertionError(f"need {count} certificate fixtures, found {len(blocks)}")
    return [block + b"\n" for block in blocks[:count]]


class UpdateCaBundleTests(unittest.TestCase):
    def test_atomic_write_uses_public_default_and_preserves_existing_mode(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "bundle.pem"

            _write_atomic(output, b"first")
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o644)

            output.chmod(0o640)
            _write_atomic(output, b"second")
            self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o640)
            self.assertEqual(output.read_bytes(), b"second")

    def test_parse_rejects_empty_source(self) -> None:
        with self.assertRaisesRegex(BundleError, "no certificates"):
            parse_certificates("empty", b"# no certificates here\n")

    def test_parse_rejects_non_utf8_source(self) -> None:
        with self.assertRaisesRegex(BundleError, "not UTF-8"):
            parse_certificates("non-ascii", b"# \xff\n")

    def test_parse_rejects_non_ascii_pem(self) -> None:
        with self.assertRaisesRegex(BundleError, "invalid certificate"):
            parse_certificates(
                "non-ascii-pem",
                "-----BEGIN CERTIFICATE-----\nÅAAA\n"
                "-----END CERTIFICATE-----\n".encode("utf-8"),
            )

    def test_parse_rejects_unbalanced_markers(self) -> None:
        with self.assertRaisesRegex(BundleError, "unbalanced"):
            parse_certificates("broken", b"-----BEGIN CERTIFICATE-----\nAAAA\n")

    def test_parse_accepts_trailing_marker_whitespace(self) -> None:
        certificate = repository_fixture_certificates(1)[0]
        source = certificate.replace(
            b"-----BEGIN CERTIFICATE-----",
            b"-----BEGIN CERTIFICATE----- \t",
        ).replace(
            b"-----END CERTIFICATE-----",
            b"-----END CERTIFICATE-----  ",
        )

        parsed = parse_certificates("fixed-layout", source)

        self.assertEqual(len(parsed), 1)

    def test_parse_accepts_trailing_base64_whitespace(self) -> None:
        lines = repository_fixture_certificates(1)[0].splitlines()
        source = b"\n".join(
            [lines[0], *(line + b" " for line in lines[1:-1]), lines[-1], b""]
        )

        parsed = parse_certificates("fixed-layout", source)

        self.assertEqual(len(parsed), 1)

    def test_parse_rejects_non_base64_characters(self) -> None:
        with self.assertRaisesRegex(BundleError, "invalid certificate"):
            parse_certificates(
                "bad-base64",
                b"-----BEGIN CERTIFICATE-----\nAAAA*\n-----END CERTIFICATE-----\n",
            )

    def test_parse_rejects_invalid_der(self) -> None:
        with self.assertRaisesRegex(BundleError, "invalid certificate"):
            parse_certificates(
                "bad-der",
                b"-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n",
            )

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

    def test_generation_preserves_first_source_bytes_as_fixed_prefix(self) -> None:
        first, second, third = repository_fixture_certificates(3)
        fixed_source = (
            b"# Fixed root 1\r\n"
            + first.replace(b"\n", b"\r\n")
            + b"# Fixed root 2\r\n"
            + second.replace(b"\n", b"\r\n")
        )

        generated = generate_bundle_bytes(
            (("Sony/Vita", fixed_source), ("Mozilla/curl", third))
        )

        self.assertEqual(generated[: len(fixed_source)], fixed_source)

    def test_repository_sony_source_matches_fixed_373_libssl_layout(self) -> None:
        source = SONY_SOURCE.read_bytes()

        self.assertEqual(hashlib.sha256(source).hexdigest(), STOCK_373_CA_LIST_SHA256)
        self.assertEqual(source.find(b"-----BEGIN CERTIFICATE-----"), 20)
        self.assertEqual(len(PEM_RE.findall(source)), 67)

    def test_repository_bundle_is_reproducible(self) -> None:
        sources = (
            ("Sony/Vita", SONY_SOURCE.read_bytes()),
            ("Mozilla/curl", MOZILLA_SOURCE.read_bytes()),
        )

        generated = generate_bundle_bytes(sources)

        self.assertEqual(generated, CA_BUNDLE.read_bytes())

    def test_repository_bundle_has_unique_certificates_and_preserves_sony_prefix(self) -> None:
        sony_bytes = SONY_SOURCE.read_bytes()
        combined_bytes = CA_BUNDLE.read_bytes()
        sony = parse_certificates("Sony/Vita", sony_bytes)
        combined = parse_certificates("combined", combined_bytes)
        fingerprints = [certificate.der_sha256 for certificate in combined]

        self.assertEqual(combined_bytes[: len(sony_bytes)], sony_bytes)
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

    def test_rendered_header_records_mozilla_certificate_date(self) -> None:
        first = repository_fixture_certificates(1)[0]
        source = (
            b"## Certificate data from Mozilla as of: Tue Oct 06 03:12:01 2026 GMT\n"
            + first
        )

        rendered = generate_bundle_bytes((("Mozilla/curl", source),))

        self.assertIn(
            b"##   Certificate data date: Tue Oct 06 03:12:01 2026 GMT\n",
            rendered,
        )

    def test_build_rejects_non_ascii_mozilla_certificate_date(self) -> None:
        first = repository_fixture_certificates(1)[0]
        source = (
            "## Certificate data from Mozilla as of: 6 octobre 2026 Å\n".encode(
                "utf-8"
            )
            + first
        )

        with self.assertRaisesRegex(BundleError, "date is not ASCII"):
            build_bundle((("Mozilla/curl", source),))


if __name__ == "__main__":
    unittest.main()
